/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "cardreader.h"

#include "ultralcd.h"
#include "stepper.h"
#include "language.h"

#include "Marlin.h"

#if ENABLED(SDSUPPORT)

char TFTresumingflag=0;
#if defined(OutageTest)
extern unsigned char PowerTestFlag;
extern char seekdataflag;
#endif
extern char TFTStatusFlag;
extern char sdcardstartprintingflag; 
extern int16_t filenumber;


CardReader::CardReader() {
  sdprinting = cardOK = saving = logging = false;
  filesize = 0;
  sdpos = 0;
  workDirDepth = 0;
  file_subcall_ctr = 0;
  ZERO(workDirParents);

  autostart_stilltocheck = true; //the SD start is delayed, because otherwise the serial cannot answer fast enough to make contact with the host software.
  autostart_index = 0;

  //power to SD reader
  #if SDPOWER > -1
    OUT_WRITE(SDPOWER, HIGH);
  #endif //SDPOWER

  next_autostart_ms = millis() + 5000;
}

void CardReader::Myls() 
{
  lsAction=MySerial3Print;
  root.rewind();
  lsDive("",root);
}

uint16_t MyFileNrCnt=0;
extern bool ReadMyfileNrFlag;
uint16_t fileoutputcnt=0;


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
void  CardReader::lsDive(const char *prepend,SdFile parent, const char * const match/*=NULL*/)
{
  dir_t p;
 uint8_t cnt=0;
  while (parent.readDir(p, longFilename) > 0)
  {
    if( DIR_IS_SUBDIR(&p) && lsAction!=LS_Count && lsAction!=LS_GetFilename) // hence LS_SerialPrint
    {

      char path[13*2];
      char lfilename[13];
      createFilename(lfilename,p);
      path[0]=0;
      if(strlen(prepend)==0) //avoid leading / if already in prepend
      {
       strcat(path,"/");
      }
      strcat(path,prepend);
      strcat(path,lfilename);
      strcat(path,"/");
      //Serial.print(path);
      
      SdFile dir;
      if(!dir.open(parent,lfilename, O_READ))
      {
        if(lsAction==LS_SerialPrint)
        {
          SERIAL_ECHO_START;
          SERIAL_ECHOLN(MSG_SD_CANT_OPEN_SUBDIR);
          SERIAL_ECHOLN(lfilename);
//          #ifdef TFTmodel
//          NEW_SERIAL_ECHOLN(MSG_SD_CANT_OPEN_SUBDIR);
//          NEW_SERIAL_ECHOLN(lfilename);
//          #endif
        }
      }
      lsDive(path,dir);
      //close done automatically by destructor of SdFile

      
    }
    else
    {
      if (p.name[0] == DIR_NAME_FREE) break;
      if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.'|| p.name[0] == '_') continue;
      if (longFilename[0] != '\0' &&
          (longFilename[0] == '.' || longFilename[0] == '_')) continue;
      if ( p.name[0] == '.')
      {
        if ( p.name[1] != '.')
        continue;
      }
      
      if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;
      filenameIsDir=DIR_IS_SUBDIR(&p);         
      if(!filenameIsDir)
      {
        if(p.name[8]!='G') continue;
        if(p.name[9]=='~') continue;
      }
      //if(cnt++!=nr) continue;
      createFilename(filename,p);
     
     if(lsAction==MySerial3Print)
     {
      if(ReadMyfileNrFlag)
        {
        if((strstr(filename,".gco")!=NULL)||(strstr(filename,".GCO")!=NULL))MyFileNrCnt++;
	//  MyFileNrCnt++;
        }
      else 
      {
      //  if((MyFileNrCnt-filenumber*4)<4)
        if((MyFileNrCnt-filenumber)<4)
        {
          if(fileoutputcnt<(MyFileNrCnt-filenumber))
          {
                                          NEW_SERIAL_PROTOCOL(prepend);
            NEW_SERIAL_PROTOCOLLN(filename);
                                  //        NEW_SERIAL_PROTOCOL(prepend);
            NEW_SERIAL_PROTOCOLLN(longFilename);              
          }
        }
    //    else if((fileoutputcnt>=((MyFileNrCnt-4)-filenumber*4))&&(fileoutputcnt<MyFileNrCnt-filenumber*4))
      else if((fileoutputcnt>=((MyFileNrCnt-4)-filenumber))&&(fileoutputcnt<MyFileNrCnt-filenumber))
        {
          NEW_SERIAL_PROTOCOL(prepend);
          NEW_SERIAL_PROTOCOLLN(filename);
                            //      NEW_SERIAL_PROTOCOL(prepend);
          NEW_SERIAL_PROTOCOLLN(longFilename);                    
        } 
        fileoutputcnt++;     	
      }
      if(fileoutputcnt>=MyFileNrCnt) fileoutputcnt=0;    




  
     }
      else if(lsAction==LS_SerialPrint)    
      {   
        SERIAL_PROTOCOL(prepend);
        SERIAL_PROTOCOLLN(filename);
      }
      else if(lsAction==LS_Count)
      {
        nrFiles++;
      } 
      else if(lsAction==LS_GetFilename)
      {
      //  if(cnt==nrFiles)
      //    return;
     //   cnt++;      
          createFilename(filename, p);
          if (match != NULL) {
            if (strcasecmp(match, filename) == 0) return;
          }
          else if (cnt == nrFiles) return;
          cnt++;

       
      }
    }
  }
}

void CardReader::ls()  {
  lsAction = LS_SerialPrint;
  root.rewind();
  lsDive("", root);
}

#if ENABLED(LONG_FILENAME_HOST_SUPPORT)

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
  cardOK = false;
  if (root.isOpen()) root.close();

  #ifndef SPI_SPEED
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
  /**
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

void CardReader::openAndPrintFile(const char *name) {
  char cmd[4 + strlen(name) + 1]; // Room for "M23 ", filename, and null
  sprintf_P(cmd, PSTR("M23 %s"), name);
  for (char *c = &cmd[4]; *c; c++) *c = tolower(*c);
  enqueue_and_echo_command(cmd);
  enqueue_and_echo_commands_P(PSTR("M24"));
}

void CardReader::startFileprint() {
  if(cardOK)
  {
  sdprinting = true;
  if(TFTresumingflag)
    {   
//      enquecommand_P(PSTR("G91"));  
      enqueue_and_echo_commands_P(PSTR("G1 Z-20"));
      enqueue_and_echo_commands_P(PSTR("G90"));   
      TFTresumingflag=false;
    }        
  }
}

void CardReader::stopSDPrint() {
  sdprinting = false;
  if (isFileOpen()) file.close();
}

void CardReader::TFTStopPringing()
{
  sdprinting = false;
  TFTresumingflag=false;
  sdcardstartprintingflag=false;
  closefile();
  quickstop_stepper();     
  NEW_SERIAL_PROTOCOLPGM("J16");//STOP
  TFT_SERIAL_ENTER();  
//  autotempShutdown();
  disable_x();
  disable_y();
  disable_z();
  disable_e0();  
}
void CardReader::TFTgetStatus()
{
//  if(TFTStatusFlag)
//  {
    if(cardOK)
      {
        NEW_SERIAL_PROTOCOL(itostr3(percentDone()));
      }
      else{
        NEW_SERIAL_PROTOCOLPGM("J02");
//        TFT_SERIAL_ENTER();
      }
//    TFTStatusFlag=0;
//  }
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
    while (*t && cnt < MAXPATHNAMELENGTH) { t++; cnt++; } //crawl counter forward.
  }
  if (cnt < MAXPATHNAMELENGTH - (FILENAME_LENGTH))
    file.getFilename(t);
  else
    t[0] = 0;
}

void CardReader::openFile(char* name, bool read, bool push_current/*=false*/) {

  if (!cardOK) return;

  uint8_t doing = 0;
  if (isFileOpen()) { //replacing current file by new file, or subfile call
    if (push_current) {
      if (file_subcall_ctr > SD_PROCEDURE_DEPTH - 1) {
        SERIAL_ERROR_START;
        SERIAL_ERRORPGM("trying to call sub-gcode files with too many levels. MAX level is:");
        SERIAL_ERRORLN(SD_PROCEDURE_DEPTH);
        kill(PSTR(MSG_KILLED));
        return;
      }
     #ifdef TFTmodel
//     NEW_SERIAL_ECHOPGM("SUBROUTINE CALL target:\"");
     NEW_SERIAL_ECHO(name);
//     NEW_SERIAL_ECHOPGM("\" parent:\"");     
     #endif
      // Store current filename and position
      getAbsFilename(proc_filenames[file_subcall_ctr]);

      SERIAL_ECHO_START;
      SERIAL_ECHOPAIR("SUBROUTINE CALL target:\"", name);
      SERIAL_ECHOPAIR("\" parent:\"", proc_filenames[file_subcall_ctr]);
      SERIAL_ECHOLNPAIR("\" pos", sdpos);
      filespos[file_subcall_ctr] = sdpos;
      file_subcall_ctr++;
    }
    else {
      doing = 1;
    }
  }
  else { // Opening fresh file
    doing = 2;
    file_subcall_ctr = 0; // Reset procedure depth in case user cancels print while in procedure
     #ifdef TFTmodel
//    NEW_SERIAL_ECHOPGM("Now fresh file: ");
    NEW_SERIAL_ECHOLN(name);
    #endif
  }

  if (doing) {
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Now ");
    SERIAL_ECHO(doing == 1 ? "doing" : "fresh");
    SERIAL_ECHOLNPAIR(" file: ", name);
  }

  stopSDPrint();

  SdFile myDir;
  curDir = &root;
  char *fname = name;
  char *dirname_start, *dirname_end;

  if (name[0] == '/') {
    dirname_start = &name[1];
    while (dirname_start != NULL) {
      dirname_end = strchr(dirname_start, '/');
      //SERIAL_ECHOPGM("start:");SERIAL_ECHOLN((int)(dirname_start - name));
      //SERIAL_ECHOPGM("end  :");SERIAL_ECHOLN((int)(dirname_end - name));
      if (dirname_end != NULL && dirname_end > dirname_start) {
        char subdirname[FILENAME_LENGTH];
        strncpy(subdirname, dirname_start, dirname_end - dirname_start);
        subdirname[dirname_end - dirname_start] = 0;
        SERIAL_ECHOLN(subdirname);
        if (!myDir.open(curDir, subdirname, O_READ)) {
          SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
          SERIAL_PROTOCOL(subdirname);
          SERIAL_PROTOCOLCHAR('.');
           #ifdef TFTmodel
      //    NEW_SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
          NEW_SERIAL_PROTOCOLPGM("J21");//OPEN FAIL
          TFT_SERIAL_ENTER();
      //    NEW_SERIAL_PROTOCOL(subdirname);
        //  NEW_SERIAL_PROTOCOLLNPGM(".");
          #endif         
          return;
        }
        else {
          //SERIAL_ECHOLNPGM("dive ok");
        }

        curDir = &myDir;
        dirname_start = dirname_end + 1;
      }
      else { // the remainder after all /fsa/fdsa/ is the filename
        fname = dirname_start;
        //SERIAL_ECHOLNPGM("remainder");
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
      SERIAL_PROTOCOLPAIR(MSG_SD_FILE_OPENED, fname);
      SERIAL_PROTOCOLLNPAIR(MSG_SD_SIZE, filesize);
      sdpos = 0;

      SERIAL_PROTOCOLLNPGM(MSG_SD_FILE_SELECTED);
      #ifdef TFTmodel
    //  NEW_SERIAL_PROTOCOLPGM(MSG_SD_FILE_OPENED);
      NEW_SERIAL_PROTOCOLPGM("J20");//OPEN SUCCESS
      TFT_SERIAL_ENTER();
//      NEW_SERIAL_PROTOCOL(fname);
//      NEW_SERIAL_PROTOCOLPGM(MSG_SD_SIZE);
//      NEW_SERIAL_PROTOCOLLN(filesize);      
//      NEW_SERIAL_PROTOCOLLNPGM(MSG_SD_FILE_SELECTED);
      #endif
      getfilename(0, fname);
      lcd_setstatus(longFilename[0] ? longFilename : fname);
    }
    else {
      SERIAL_PROTOCOLPAIR(MSG_SD_OPEN_FILE_FAIL, fname);
      SERIAL_PROTOCOLCHAR('.');
      SERIAL_EOL;
       #ifdef TFTmodel
//      NEW_SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
//      NEW_SERIAL_PROTOCOL(fname);
//      NEW_SERIAL_PROTOCOLLNPGM(".");
      NEW_SERIAL_PROTOCOLPGM("J21");//OPEN FAIL
      TFT_SERIAL_ENTER();
      #endif
    }
  }
  else { //write
    if (!file.open(curDir, fname, O_CREAT | O_APPEND | O_WRITE | O_TRUNC)) {
      SERIAL_PROTOCOLPAIR(MSG_SD_OPEN_FILE_FAIL, fname);
      SERIAL_PROTOCOLCHAR('.');
      SERIAL_EOL;
       #ifdef TFTmodel
//      NEW_SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
//      NEW_SERIAL_PROTOCOL(fname);
//      NEW_SERIAL_PROTOCOLLNPGM(".");
      NEW_SERIAL_PROTOCOLPGM("J21");//OPEN FAIL
      TFT_SERIAL_ENTER();
      #endif
    }
    else {
      saving = true;
      SERIAL_PROTOCOLLNPAIR(MSG_SD_WRITE_TO_FILE, name);
      lcd_setstatus(fname);
    }
  }
}

void CardReader::removeFile(char* name) {
  if (!cardOK) return;

  stopSDPrint();

  SdFile myDir;
  curDir = &root;
  char *fname = name;

  char *dirname_start, *dirname_end;
  if (name[0] == '/') {
    dirname_start = strchr(name, '/') + 1;
    while (dirname_start != NULL) {
      dirname_end = strchr(dirname_start, '/');
      //SERIAL_ECHOPGM("start:");SERIAL_ECHOLN((int)(dirname_start - name));
      //SERIAL_ECHOPGM("end  :");SERIAL_ECHOLN((int)(dirname_end - name));
      if (dirname_end != NULL && dirname_end > dirname_start) {
        char subdirname[FILENAME_LENGTH];
        strncpy(subdirname, dirname_start, dirname_end - dirname_start);
        subdirname[dirname_end - dirname_start] = 0;
        SERIAL_ECHOLN(subdirname);
        if (!myDir.open(curDir, subdirname, O_READ)) {
          SERIAL_PROTOCOLPAIR("open failed, File: ", subdirname);
          SERIAL_PROTOCOLCHAR('.');
          SERIAL_EOL;
          return;
        }
        else {
          //SERIAL_ECHOLNPGM("dive ok");
        }

        curDir = &myDir;
        dirname_start = dirname_end + 1;
      }
      else { // the remainder after all /fsa/fdsa/ is the filename
        fname = dirname_start;
        //SERIAL_ECHOLNPGM("remainder");
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

  file.writeError = false;
  if ((npos = strchr(buf, 'N')) != NULL) {
    begin = strchr(npos, ' ') + 1;
    end = strchr(npos, '*') - 1;
  }
  end[1] = '\r';
  end[2] = '\n';
  end[3] = '\0';
  file.write(begin);
  if (file.writeError) {
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_SD_ERR_WRITE_TO_FILE);
  }
}

void CardReader::checkautostart(bool force) {
  if (!force && (!autostart_stilltocheck || ELAPSED(millis(), next_autostart_ms)))
    return;

  autostart_stilltocheck = false;

  if (!cardOK) {
    initsd();
    if (!cardOK) return; // fail
  }

  char autoname[10];
  sprintf_P(autoname, PSTR("auto%i.g"), autostart_index);
  for (int8_t i = 0; i < (int8_t)strlen(autoname); i++) autoname[i] = tolower(autoname[i]);

  dir_t p;

  root.rewind();

  bool found = false;
  while (root.readDir(p, NULL) > 0) {
    for (int8_t i = 0; i < (int8_t)strlen((char*)p.name); i++) p.name[i] = tolower(p.name[i]);
    if (p.name[9] != '~' && strncmp((char*)p.name, autoname, 5) == 0) {
      openAndPrintFile(autoname);
      found = true;
    }
  }
  if (!found)
    autostart_index = -1;
  else
    autostart_index++;
}

void CardReader::closefile(bool store_location) {
  file.sync();
  file.close();
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
  SdFile newfile;
  SdFile *parent = &root;

  if (workDir.isOpen()) parent = &workDir;

  if (!newfile.open(*parent, relpath, O_READ)) {
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM(MSG_SD_CANT_ENTER_SUBDIR);
    SERIAL_ECHOLN(relpath);
  }
  else {
    if (workDirDepth < MAX_DIR_DEPTH)
      workDirParents[workDirDepth++] = *parent;
    workDir = newfile;
  }
}

void CardReader::updir() {
  if (workDirDepth > 0)
    workDir = workDirParents[--workDirDepth];
}

void CardReader::printingHasFinished() {
  stepper.synchronize();
  file.close();
  if (file_subcall_ctr > 0) { // Heading up to a parent file that called current as a procedure.
    file_subcall_ctr--;
    openFile(proc_filenames[file_subcall_ctr], true, true);
    setIndex(filespos[file_subcall_ctr]);
    startFileprint();
  }
  else {
    sdprinting = false;
    if (SD_FINISHED_STEPPERRELEASE)
      enqueue_and_echo_commands_P(PSTR(SD_FINISHED_RELEASECOMMAND));
    print_job_timer.stop();
    if (print_job_timer.duration() > 60)
      enqueue_and_echo_commands_P(PSTR("M31"));
      
  #if defined(OutageTest)
    PowerTestFlag=false;
    seekdataflag=0;
    WRITE(OUTAGECON_PIN,LOW);
    FlagResumFromOutage=0;
    #endif
    #if defined(TFTmodel)
    NEW_SERIAL_PROTOCOLPGM("J14");//PRINT DONE
    TFT_SERIAL_ENTER();
    powerOFFflag=1;
    #endif     
  }  
}

#endif //SDSUPPORT
