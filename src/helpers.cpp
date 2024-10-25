#include "helpers.h"

void simulateDisplayOutput(bool ledmatrix[], String front, int min){
  String toPrint = "";
  if(min > 0){
    toPrint.concat("*");
  } else {
    toPrint.concat("\n ");
  }
  if(min > 1){
    toPrint.concat("           *\n ");
  } else {
    toPrint.concat("\n ");
  }
  for(int i = 0; i < 110; i++){
    if(i % 11 == 0 && i != 0){
      toPrint.concat("\n ");
    }
    if(ledmatrix[i]){
      toPrint.concat(front[i]);
    } else {
      toPrint.concat(" ");
    }
  }
    if(min > 2){
    toPrint.concat("\n*");
  }
  if(min > 3){
    toPrint.concat("           *");
  }
  Serial.println(toPrint);
}

String readFileToString(String filename){
  String file_content;
  File file = LittleFS.open(filename, FILE_READ);
  if(!file){
    Serial.println("Failed to open file");
  } else {
      file_content = file.readString();
    //Serial.println(file_content);
    file.close();
  }
  return file_content;
}


void fileWriteData(String data, String filename){
  File file = LittleFS.open(filename, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file");
    return;
  }
  file.print(data);
  file.close();
}
