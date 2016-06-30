#include "Marlin.h"
#include "cardreader.h"
#include "ultralcd.h"
#include "stepper.h"
#include "temperature.h"
#include "language.h"

#ifdef SDHSMCI_SUPPORT
  #include <SD_HSMCI.h>
  #include "Arduino_Due_SD_HSCMI.h" // This creates the object SD
#endif

#ifdef SDSUPPORT

CardReader::CardReader() {
  filesize = 0;
  sdpos = 0;
  sdprinting = false;
  cardOK = false;
  saving = false;
  logging = false;
  workDirDepth = 0;
  file_subcall_ctr = 0;
  memset(workDirParents, 0, sizeof(workDirParents));

  #ifdef SDHSMCI_SUPPORT
    sdhsmci_printing = false;
    strcpy(current_working_directory,"0:/");
  #endif

  autostart_stilltocheck = true; //the SD start is delayed, because otherwise the serial cannot answer fast enough to make contact with the host software.
  autostart_index = 0;

  //power to SD reader
  #if SDPOWER > -1
    OUT_WRITE(SDPOWER, HIGH);
  #endif //SDPOWER

  next_autostart_ms = millis() + 5000;
}

char *createFilename(char *buffer, const dir_t &p) { //buffer > 12characters
  char *pos = buffer;
  for (uint8_t i = 0; i < 11; i++) {
    if (p.name[i] == ' ') continue;
    if (i == 8) *pos++ = '.';
    *pos++ = p.name[i];
  }
  *pos++ = 0;
  return buffer;
}

/**
 * Dive into a folder and recurse depth-first to perform a pre-set operation lsAction:
 *   LS_Count       - Add +1 to nrFiles for every file within the parent
 *   LS_GetFilename - Get the filename of the file indexed by nrFiles
 *   LS_SerialPrint - Print the full path of each file to serial output
 */
void CardReader::lsDive(const char *prepend, SdFile parent, const char * const match/*=NULL*/) {
  dir_t p;
  uint8_t cnt = 0;

  // Read the next entry from a directory
  while (parent.readDir(p, longFilename) > 0) {

    // If the entry is a directory and the action is LS_SerialPrint
    if (DIR_IS_SUBDIR(&p) && lsAction != LS_Count && lsAction != LS_GetFilename) {

      // Get the short name for the item, which we know is a folder
      char lfilename[FILENAME_LENGTH];
      createFilename(lfilename, p);

      // Allocate enough stack space for the full path to a folder, trailing slash, and nul
      boolean prepend_is_empty = (prepend[0] == '\0');
      int len = (prepend_is_empty ? 1 : strlen(prepend)) + strlen(lfilename) + 1 + 1;
      char path[len];

      // Append the FOLDERNAME12/ to the passed string.
      // It contains the full path to the "parent" argument.
      // We now have the full path to the item in this folder.
      strcpy(path, prepend_is_empty ? "/" : prepend); // root slash if prepend is empty
      strcat(path, lfilename); // FILENAME_LENGTH-1 characters maximum
      strcat(path, "/");       // 1 character

      // Serial.print(path);

      // Get a new directory object using the full path
      // and dive recursively into it.
      SdFile dir;
      if (!dir.open(parent, lfilename, O_READ)) {
        if (lsAction == LS_SerialPrint) {
          SERIAL_ECHO_START;
          SERIAL_ECHOLN(MSG_SD_CANT_OPEN_SUBDIR);
          SERIAL_ECHOLN(lfilename);
        }
      }
      lsDive(path, dir);
      // close() is done automatically by destructor of SdFile
    }
    else {
      char pn0 = p.name[0];
      if (pn0 == DIR_NAME_FREE) break;
      if (pn0 == DIR_NAME_DELETED || pn0 == '.') continue;
      if (longFilename[0] == '.') continue;

      if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;

      filenameIsDir = DIR_IS_SUBDIR(&p);

      if (!filenameIsDir && (p.name[8] != 'G' || p.name[9] == '~')) continue;

      switch (lsAction) {
        case LS_Count:
          nrFiles++;
          break;
        case LS_SerialPrint:
          createFilename(filename, p);
          SERIAL_PROTOCOL(prepend);
          SERIAL_PROTOCOLLN(filename);
          break;
        case LS_GetFilename:
          createFilename(filename, p);
          if (match != NULL) {
            if (strcasecmp(match, filename) == 0) return;
          }
          else if (cnt == nrFiles) return;
          cnt++;
          break;
      }

    }
  } // while readDir
}

#ifdef SDHSMCI_SUPPORT
void CardReader::sdhsmci_init() {
  cardOK = false;
  SD.Init();
  if(sd_mmc_check(0) == SD_MMC_OK) {
    SerialUSB.println("Debug: SDHSMCI cardOK!");
    cardOK = true;

    //file is open after init? Then re-open the previously open file.
    if(isFileOpen()) {
      uint32_t tmp_sdpos = sdpos;
      //sdhsmci_file.Close();
      openFile(&sdhsmci_filename[0],!saving); //openFile 1 for READ
      //sdhsmci_file.Open(current_working_directory,sdhsmci_filename,saving); // 0 for READ, 1 for WRITE
      sdhsmci_file.Seek(tmp_sdpos);
      sdpos = tmp_sdpos;
      SerialUSB.print("Debug Info: resuming sdpos: ");
      SerialUSB.println(tmp_sdpos);
    }
  }
}

bool CardReader::sdhsmci_eof() {
  return sdhsmci_file.Position() >= filesize;
}

void sdhsmci_list_dir() //File name list seperated by newlines.
{
  FileInfo file_info;
  if (SD.FindFirst(card.current_working_directory, file_info))
  {
      do
      {
        SerialUSB.print(file_info.fileName);
        if(file_info.isDirectory) SerialUSB.print("/");
        SerialUSB.println();
      }
      while (SD.FindNext(file_info));
  }
}
#endif

void CardReader::ls()  {
 #ifdef SDHSMCI_SUPPORT
  sdhsmci_list_dir();
 #else
  lsAction = LS_SerialPrint;
  root.rewind();
  lsDive("", root);
 #endif
}

#ifdef LONG_FILENAME_HOST_SUPPORT

  /**
   * Get a long pretty path based on a DOS 8.3 path
   */
  void CardReader::printLongPath(char *path) {
    lsAction = LS_GetFilename;

    int i, pathLen = strlen(path);

    // SERIAL_ECHOPGM("Full Path: "); SERIAL_ECHOLN(path);

    // Zero out slashes to make segments
    for (i = 0; i < pathLen; i++) if (path[i] == '/') path[i] = '\0';

    SdFile diveDir = root; // start from the root for segment 1
    for (i = 0; i < pathLen;) {

      if (path[i] == '\0') i++; // move past a single nul

      char *segment = &path[i]; // The segment after most slashes

      // If a segment is empty (extra-slash) then exit
      if (!*segment) break;

      // Go to the next segment
      while (path[++i]) { }

      // SERIAL_ECHOPGM("Looking for segment: "); SERIAL_ECHOLN(segment);

      // Find the item, setting the long filename
      diveDir.rewind();
      lsDive("", diveDir, segment);

      // Print /LongNamePart to serial output
      SERIAL_PROTOCOLCHAR('/');
      SERIAL_PROTOCOL(longFilename[0] ? longFilename : "???");

      // If the filename was printed then that's it
      if (!filenameIsDir) break;

      // SERIAL_ECHOPGM("Opening dir: "); SERIAL_ECHOLN(segment);

      // Open the sub-item as the new dive parent
      SdFile dir;
      if (!dir.open(diveDir, segment, O_READ)) {
        SERIAL_EOL;
        SERIAL_ECHO_START;
        SERIAL_ECHOPGM(MSG_SD_CANT_OPEN_SUBDIR);
        SERIAL_ECHO(segment);
        break;
      }

      diveDir.close();
      diveDir = dir;

    } // while i<pathLen

    SERIAL_EOL;
  }

#endif // LONG_FILENAME_HOST_SUPPORT

void CardReader::initsd() {
  #ifdef SDHSMCI_SUPPORT
    sdhsmci_init();
    return;
  #endif
  cardOK = false;
  if (root.isOpen()) root.close();

  #ifdef SDSLOW
    #define SPI_SPEED SPI_HALF_SPEED
  #else
    #define SPI_SPEED SPI_FULL_SPEED
  #endif

  if (!card.init(SPI_SPEED,SDSS)
    #if defined(LCD_SDSS) && (LCD_SDSS != SDSS)
      && !card.init(SPI_SPEED, LCD_SDSS)
    #endif
  ) {
    //if (!card.init(SPI_HALF_SPEED,SDSS))
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM(MSG_SD_INIT_FAIL);
  }
  else if (!volume.init(&card)) {
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_SD_VOL_INIT_FAIL);
  }
  else if (!root.openRoot(&volume)) {
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_SD_OPENROOT_FAIL);
  }
  else {
    cardOK = true;
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM(MSG_SD_CARD_OK);
  }
  workDir = root;
  curDir = &root;
  /*
  if (!workDir.openRoot(&volume)) {
    SERIAL_ECHOLNPGM(MSG_SD_WORKDIR_FAIL);
  }
  */
}

void CardReader::setroot() {
  /*if (!workDir.openRoot(&volume)) {
    SERIAL_ECHOLNPGM(MSG_SD_WORKDIR_FAIL);
  }*/
  workDir = root;
  curDir = &workDir;
}

void CardReader::release() {
  sdprinting = false;
  cardOK = false;
}

void CardReader::startFileprint() {
  if (cardOK) {
    sdprinting = true;
  }
}

void CardReader::pauseSDPrint() {
  if (sdprinting) sdprinting = false;
}

void CardReader::openLogFile(char* name) {
  logging = true;
  openFile(name, false);
}

void CardReader::getAbsFilename(char *t) {
  uint8_t cnt = 0;
  *t = '/'; t++; cnt++;
  for (uint8_t i = 0; i < workDirDepth; i++) {
    workDirParents[i].getFilename(t); //SDBaseFile.getfilename!
    while(*t && cnt < MAXPATHNAMELENGTH) { t++; cnt++; } //crawl counter forward.
  }
  if (cnt < MAXPATHNAMELENGTH - FILENAME_LENGTH)
    file.getFilename(t);
  else
    t[0] = 0;
}

#ifdef SDHSMCI_SUPPORT
void up_directory(char *path) {
  #define ROOT_MIN_LEN 3 //will preserve these chars
  for(size_t i = strlen(path)-2; i >= ROOT_MIN_LEN; i--) { //-2 assumes path has ending slash
    if( path[i] == '/') break;
    path[i] = 0;
  }
}

bool CardReader::sdhsmci_is_subdirectory() {
  return strlen(current_working_directory) > ROOT_MIN_LEN;
}

void CardReader::sdhsmci_updir() {
  up_directory(current_working_directory);
}

void CardReader::sdhsmci_open_file(char* name, bool read) {
  /*
  if( strcmp(name,"..") ) {
    up_directory(current_working_directory);
    return;
  }
  */
  
  //Check if selected file is a directory.
  if(name[strlen(name)-1] == '/') {
    uint8_t current_dir_len = strlen(current_working_directory);
    //Check path is not too long.
    if( (strlen(name) + strlen(current_working_directory)) > 256 ) {
      SerialUSB.println(PSTR("Directory change failed: Over MAXDIR_LENTH."));
      return;
    }
    
    //char *current_dir = &current_working_directory[ strlen(current_working_directory) ];
    //Append directory to current working directory.
    strcat(current_working_directory, name);
    SerialUSB.print(PSTR("Current Working Directory: "));
    SerialUSB.println(current_working_directory);
    return;
  }

  if(isFileOpen()) closefile();
  sdhsmci_file.Open(current_working_directory,name,!read); // 0 for READ, 1 for WRITE
  SerialUSB.print(PSTR("Debug Info: filesize: "));
  SerialUSB.println(sdhsmci_file.Length());
  SerialUSB.print(PSTR("Debug Info: Status: "));
  SerialUSB.println(sdhsmci_file.Status());
  SerialUSB.print(PSTR("Debug Info: inUse: "));
  SerialUSB.println(sdhsmci_file.inUse);
  if(read) filesize = sdhsmci_file.Length();
  if(!read) {
    saving = true;
    SERIAL_PROTOCOLPGM(MSG_SD_WRITE_TO_FILE);
    SERIAL_PROTOCOLLN(name);
    lcd_setstatus(name);
  }
  sdpos = 0;
}
#endif

void CardReader::openFile(char* name, bool read, bool replace_current/*=true*/) {
  if (!cardOK) return;
  #ifdef SDHSMCI_SUPPORT
    strcpy(sdhsmci_filename,name); //store filename for recovery from a re-init of sd.
    sdhsmci_open_file(name,read);
    return;
  #endif
  if (file.isOpen()) { //replacing current file by new file, or subfile call
    if (!replace_current) {
     if (file_subcall_ctr > SD_PROCEDURE_DEPTH - 1) {
       SERIAL_ERROR_START;
       SERIAL_ERRORPGM("trying to call sub-gcode files with too many levels. MAX level is:");
       SERIAL_ERRORLN(SD_PROCEDURE_DEPTH);
       kill(PSTR(MSG_KILLED));
       return;
     }

     SERIAL_ECHO_START;
     SERIAL_ECHOPGM("SUBROUTINE CALL target:\"");
     SERIAL_ECHO(name);
     SERIAL_ECHOPGM("\" parent:\"");

     //store current filename and position
     getAbsFilename(filenames[file_subcall_ctr]);

     SERIAL_ECHO(filenames[file_subcall_ctr]);
     SERIAL_ECHOPGM("\" pos");
     SERIAL_ECHOLN(sdpos);
     filespos[file_subcall_ctr] = sdpos;
     file_subcall_ctr++;
    }
    else {
     SERIAL_ECHO_START;
     SERIAL_ECHOPGM("Now doing file: ");
     SERIAL_ECHOLN(name);
    }
    file.close();
  }
  else { //opening fresh file
    file_subcall_ctr = 0; //resetting procedure depth in case user cancels print while in procedure
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Now fresh file: ");
    SERIAL_ECHOLN(name);
  }
  sdprinting = false;

  SdFile myDir;
  curDir = &root;
  char *fname = name;

  char *dirname_start, *dirname_end;
  if (name[0] == '/') {
    dirname_start = &name[1];
    while (dirname_start > 0) {
      dirname_end = strchr(dirname_start, '/');
      //SERIAL_ECHO("start:");SERIAL_ECHOLN((int)(dirname_start - name));
      //SERIAL_ECHO("end  :");SERIAL_ECHOLN((int)(dirname_end - name));
      if (dirname_end > 0 && dirname_end > dirname_start) {
        char subdirname[FILENAME_LENGTH];
        strncpy(subdirname, dirname_start, dirname_end - dirname_start);
        subdirname[dirname_end - dirname_start] = 0;
        SERIAL_ECHOLN(subdirname);
        if (!myDir.open(curDir, subdirname, O_READ)) {
          SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
          SERIAL_PROTOCOL(subdirname);
          SERIAL_PROTOCOLCHAR('.');
          return;
        }
        else {
          //SERIAL_ECHOLN("dive ok");
        }

        curDir = &myDir;
        dirname_start = dirname_end + 1;
      }
      else { // the remainder after all /fsa/fdsa/ is the filename
        fname = dirname_start;
        //SERIAL_ECHOLN("remainder");
        //SERIAL_ECHOLN(fname);
        break;
      }
    }
  }
  else { //relative path
    curDir = &workDir;
  }

  if (read) {
    if (file.open(curDir, fname, O_READ)) {
      filesize = file.fileSize();
      SERIAL_PROTOCOLPGM(MSG_SD_FILE_OPENED);
      SERIAL_PROTOCOL(fname);
      SERIAL_PROTOCOLPGM(MSG_SD_SIZE);
      SERIAL_PROTOCOLLN(filesize);
      sdpos = 0;

      SERIAL_PROTOCOLLNPGM(MSG_SD_FILE_SELECTED);
      getfilename(0, fname);
      lcd_setstatus(longFilename[0] ? longFilename : fname);
    }
    else {
      SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
      SERIAL_PROTOCOL(fname);
      SERIAL_PROTOCOLPGM(".\n");
    }
  }
  else { //write
    if (!file.open(curDir, fname, O_CREAT | O_APPEND | O_WRITE | O_TRUNC)) {
      SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
      SERIAL_PROTOCOL(fname);
      SERIAL_PROTOCOLPGM(".\n");
    }
    else {
      saving = true;
      SERIAL_PROTOCOLPGM(MSG_SD_WRITE_TO_FILE);
      SERIAL_PROTOCOLLN(name);
      lcd_setstatus(fname);
    }
  }
}

void CardReader::removeFile(char* name) {
  #ifdef SDHSMCI_SUPPORT
    return;
  #endif
  if (!cardOK) return;

  file.close();
  sdprinting = false;

  SdFile myDir;
  curDir = &root;
  char *fname = name;

  char *dirname_start, *dirname_end;
  if (name[0] == '/') {
    dirname_start = strchr(name, '/') + 1;
    while (dirname_start > 0) {
      dirname_end = strchr(dirname_start, '/');
      //SERIAL_ECHO("start:");SERIAL_ECHOLN((int)(dirname_start - name));
      //SERIAL_ECHO("end  :");SERIAL_ECHOLN((int)(dirname_end - name));
      if (dirname_end > 0 && dirname_end > dirname_start) {
        char subdirname[FILENAME_LENGTH];
        strncpy(subdirname, dirname_start, dirname_end - dirname_start);
        subdirname[dirname_end - dirname_start] = 0;
        SERIAL_ECHOLN(subdirname);
        if (!myDir.open(curDir, subdirname, O_READ)) {
          SERIAL_PROTOCOLPGM("open failed, File: ");
          SERIAL_PROTOCOL(subdirname);
          SERIAL_PROTOCOLCHAR('.');
          return;
        }
        else {
          //SERIAL_ECHOLN("dive ok");
        }

        curDir = &myDir;
        dirname_start = dirname_end + 1;
      }
      else { // the remainder after all /fsa/fdsa/ is the filename
        fname = dirname_start;
        //SERIAL_ECHOLN("remainder");
        //SERIAL_ECHOLN(fname);
        break;
      }
    }
  }
  else { // relative path
    curDir = &workDir;
  }

  if (file.remove(curDir, fname)) {
    SERIAL_PROTOCOLPGM("File deleted:");
    SERIAL_PROTOCOLLN(fname);
    sdpos = 0;
  }
  else {
    SERIAL_PROTOCOLPGM("Deletion failed, File: ");
    SERIAL_PROTOCOL(fname);
    SERIAL_PROTOCOLCHAR('.');
  }
}

void CardReader::getStatus() {
  if (cardOK) {
    SERIAL_PROTOCOLPGM(MSG_SD_PRINTING_BYTE);
    SERIAL_PROTOCOL(sdpos);
    SERIAL_PROTOCOLCHAR('/');
    SERIAL_PROTOCOLLN(filesize);
  }
  else {
    SERIAL_PROTOCOLLNPGM(MSG_SD_NOT_PRINTING);
  }
}

void CardReader::write_command(char *buf) {
  char* begin = buf;
  char* npos = 0;
  char* end = buf + strlen(buf) - 1;

  #ifndef SDHSMCI_SUPPORT
  file.writeError = false;
  #endif
  if ((npos = strchr(buf, 'N')) != NULL) {
    begin = strchr(npos, ' ') + 1;
    end = strchr(npos, '*') - 1;
  }
  end[1] = '\r';
  end[2] = '\n';
  end[3] = '\0';
  #ifdef SDHSMCI_SUPPORT
    if (!sdhsmci_file.Write(begin)) { // Invert the return valure for error checking, Write() returns false if there is an error
  #else
  file.write(begin);
  if (file.writeError) {
  #endif
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_SD_ERR_WRITE_TO_FILE);
  }
}

void CardReader::checkautostart(bool force) {
  if (!force && (!autostart_stilltocheck || next_autostart_ms < millis()))
    return;

  autostart_stilltocheck = false;

  if (!cardOK) {
    initsd();
    if (!cardOK) return; // fail
  }
  #ifdef SDHSMCI_SUPPORT
    return;
  #endif

  char autoname[10];
  sprintf_P(autoname, PSTR("auto%i.g"), autostart_index);
  for (int8_t i = 0; i < (int8_t)strlen(autoname); i++) autoname[i] = tolower(autoname[i]);

  dir_t p;

  root.rewind();

  bool found = false;
  while (root.readDir(p, NULL) > 0) {
    for (int8_t i = 0; i < (int8_t)strlen((char*)p.name); i++) p.name[i] = tolower(p.name[i]);
    if (p.name[9] != '~' && strncmp((char*)p.name, autoname, 5) == 0) {
      char cmd[4 + (FILENAME_LENGTH + 1) * MAX_DIR_DEPTH + 2];
      sprintf_P(cmd, PSTR("M23 %s"), autoname);
      enqueuecommand(cmd);
      enqueuecommands_P(PSTR("M24"));
      found = true;
    }
  }
  if (!found)
    autostart_index = -1;
  else
    autostart_index++;
}

void CardReader::closefile(bool store_location) {
  #ifdef SDHSMCI_SUPPORT
    sdhsmci_file.Close();
  #else
    file.sync();
    file.close();
  #endif
    saving = logging = false;

    if (store_location) {
      //future: store printer state, filename and position for continuing a stopped print
      // so one can unplug the printer and continue printing the next day.
    }
}

/**
 * Get the name of a file in the current directory by index
 */
void CardReader::getfilename(uint16_t nr, const char * const match/*=NULL*/) {
  curDir = &workDir;
  lsAction = LS_GetFilename;
  nrFiles = nr;
  curDir->rewind();
  lsDive("", *curDir, match);
}

uint16_t CardReader::getnrfilenames() {
  curDir = &workDir;
  lsAction = LS_Count;
  nrFiles = 0;
  curDir->rewind();
  lsDive("", *curDir);
  //SERIAL_ECHOLN(nrFiles);
  return nrFiles;
}

void CardReader::chdir(const char * relpath) {
  #ifdef SDHSMCI_SUPPORT
  //if(name[strlen(name)-1] == '/') {
    char *name = (char *)relpath;
    uint8_t current_dir_len = strlen(current_working_directory);
    //Check path is not too long.
    if( (strlen(name) + strlen(current_working_directory)) > 256 ) {
      SerialUSB.println(PSTR("Directory change failed: Over MAXDIR_LENTH."));
      return;
    }
    
    //char *current_dir = &current_working_directory[ strlen(current_working_directory) ];
    //Append directory to current working directory.
    strcat(current_working_directory, name);
    current_dir_len = strlen(current_working_directory);
    if(current_working_directory[current_dir_len] != '/') strcat(current_working_directory,"/");
    SerialUSB.print(PSTR("Current Working Directory: ")); SerialUSB.println(current_working_directory);
    return;
  
  /*
    strcat(path,"/");
    SerialUSB.print(PSTR("Debug menu_action_chdir(): "));
    SerialUSB.println(path);

    openFile( path, 1); */
    return;
  #endif
  SdFile newfile;
  SdFile *parent = &root;

  if (workDir.isOpen()) parent = &workDir;

  if (!newfile.open(*parent, relpath, O_READ)) {
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM(MSG_SD_CANT_ENTER_SUBDIR);
    SERIAL_ECHOLN(relpath);
  }
  else {
    if (workDirDepth < MAX_DIR_DEPTH) {
      ++workDirDepth;
      for (int d = workDirDepth; d--;) workDirParents[d + 1] = workDirParents[d];
      workDirParents[0] = *parent;
    }
    workDir = newfile;
  }
}

void CardReader::updir() {
  if (workDirDepth > 0) {
    --workDirDepth;
    workDir = workDirParents[0];
    for (uint16_t d = 0; d < workDirDepth; d++)
      workDirParents[d] = workDirParents[d+1];
  }
}

void CardReader::printingHasFinished() {
  st_synchronize();
  if (file_subcall_ctr > 0) { // Heading up to a parent file that called current as a procedure.
    file.close();
    file_subcall_ctr--;
    openFile(filenames[file_subcall_ctr], true, true);
    setIndex(filespos[file_subcall_ctr]);
    startFileprint();
  }
  else {
    closefile();
    sdprinting = false;
    if (SD_FINISHED_STEPPERRELEASE) {
      //finishAndDisableSteppers();
      enqueuecommands_P(PSTR(SD_FINISHED_RELEASECOMMAND));
    }
    autotempShutdown();
  }
}

#endif //SDSUPPORT
