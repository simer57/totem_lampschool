/*versione Rfid522readerfin25 
* 
* @author Silvio Merla
* @license Released into the public domain.
*
*/
#include <avr/wdt.h>
#include <SD.h>
#include <SPI.h>
#include <RFID.h>
#include <Ethernet2.h>
//#include <Ethernet.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#define	uchar	unsigned char
#define	uint	unsigned int
#include <pitches.h>
#include <MD5.h>

//impostazione WD
const byte       _8sec = B01100001;  //interrupt
const byte    _8secRes = B00101001;  //reset
const byte _8secIntRes = B01101001;  //int + reset
const byte       _4sec = B01100000;  //interrupt
const byte    _4secRes = B00101000;  //reset
const byte    _2secRes = B00001111;  //reset 
const byte _250msecRes = B00001100;  //reset
//impostazioni RFID RC522
const int chipSelectRfid = 53;   //selezione rfid
const int NRSTPD = 48;          //reset rfid
//indirizzi di salvataggio setup in eeprom
const int a_istituto = 0;
const int a_orarioLezioni  = 25;
const int a_mRitIngresso = 54;
const int a_ipWebServer = 55;
const int a_1byteIP = 103;
//const int a_2byteIP = 104;
//const int a_3byteIP = 105;
//const int a_4byteIP = 106;
const int a_1bytegateway = 107;
//const int a_2bytegateway = 108;
//const int a_3bytegateway = 109;
//const int a_4bytegateway = 110;
const int a_memLegale = 111;
const int a_dirWebServer = 115; //occupati max 50 caratteri
const int a_dir2WebServer = 165; //occupati max 50 caratteri
const int a_mRitTx = 216;
const int a_dPO = 217;
const int a_aggClass = 218;
const int a_maskRegAbil = 219;
const int a_durata = 220;
const int a_disattMin = 221;
const int a_selFile = 222;
const int a_dir3WebServer = 230; //occupati max 50 caratteri
const int a_md5 = 290;   //occupati 33 caratteri

//byte mac[] = { 0x90, 0xA2, 0xDA, 0x0B, 0x00, 0xB4 };
byte mac[] = { 0x90, 0xA2, 0xDA, 0x10, 0x0B, 0x56 };//modificare secondo le proprie esigenze
byte ipReset[] = { 192, 168, 0, 254 };
byte gateway[] = { 192, 168, 0, 1 }; 
byte subnet[] = { 255, 255, 255, 0 }; 
byte dnsq[] = { 8, 8, 8, 8 }; //  Google Dns per evitare di programmarlo
char serverCK[] = "www.google.it"; //

/////////////////////////////////////////////////////////////////////
//set the pin
/////////////////////////////////////////////////////////////////////
const byte chipSelectEth = 10;   //pin selezione Ethernet
const byte chipSelectSD = 4;   //pin selezione SD
const byte pulsanteTest = 39; //pulsante per test
const byte capacitor = 37;

//set constant///////////////////////////////////////////////////
const byte iProgrPresenze = 72; //indice posizione nel vettore blockdata
const byte iProgrGiorni = 73; //indice posizione nel vettore blockdata
const byte iProgrRitardi = 74; //indice posizione nel vettore blockdata
const byte iFuorisede = 47;  //indice posizione nel vettore blockdata
const byte iRegistro = 75;

const byte buz = 46;   //buzzer
const byte led1 = 29;
const byte led2 = 31;
const byte led3 = 33;
const byte led4 = 35;

//per il clock//////////////////////////////////////////////////

byte giorno_sett, giorno_mese, mese, anno;
volatile byte secondi, minuti, ora;
volatile boolean sem = false; //semaforo per visualizzare solo in uscita dal interrupt
volatile short timer2min = 0; //serve per ripristinare visualizzazione su display prime 3 righe
volatile byte contatoreNTx;
volatile boolean trasmettiWS = false;  //si attiva per trasmettere al web server
volatile boolean newRecSD = false; //si setta se ci sono nuove registrazioni nella SD da trasmettere e si resetta appena trasmesse
volatile boolean orarioIngressoRitardo = false;  //si attiva dopo l'orario inizio 1^ ora + mRitIngresso
volatile bool disattivaPrimaOra; //variabile da programmare con setup per attivare variabile successiva
volatile bool primaOra = false; // se true tra le 8.13 e le 9.00 sistema non registra ritardi
volatile byte minutiExtra = 0; //minuti extra orario estemporaneo
volatile word oraInMinuti, maxOrarioIngresso;
volatile byte  mRitIngresso, mRitTx;
volatile byte durata;   //durata in minuto del periodo di lezione (tipicamente 60 min)
volatile byte disattMin; //durata in minuti disattivazione dopo le prime 3 ore (tipicamente 30 min)
volatile word orarioLezioni[2][3];
volatile bool nextDay = false;
volatile bool unaTx = false;   //tra le 7.55 e le 8.10 effettua una trasmissione ad ogni minuto di 30 registrazioni per non perdere tempo in cicli
bool nonAttivo = false;
String dataDisplay; //stringa per visualizzare su display la data
uchar  status;     //RFID
uchar RC_size;     //RFID
uchar serNum[5];      //4 bytes Serial number of card, the 5 bytes is verfiy bytes
RFID rfid(chipSelectRfid, NRSTPD); 
LiquidCrystal lcd(8, 9, 17, 16, 15, 14);

int totPresenzeGiorno = 0; //totalizzatore presenze giorno

int progrPresenze; //variabile che contiene in transito il campo progr presenze (n. giorni di presenza letto da scheda)
int progrRitardi; //variabile che contiene in transito il campo progr ritardi(n. ritardi letto da scheda)
int progrGiorni; //contatore giornaliero per determinare se si timbra in ingresso o in uscita
char filePresenze[9];
char * filepres = &filePresenze[0];
int puntFilePres;  //contiene il primo byte da trasmettere del file presenze
String gSett;     //stringa giorno settimana per visualizzazione
boolean coldStart;
boolean noCard = true;
boolean cardRipassata = false;
boolean ritardo = false;
boolean leggiSetup = false;
boolean leggiProgr = false;
boolean setupOK = false;
boolean progrOK = false;
boolean richiestiDatiWeb = false;  //si attiva dopo richiesta autorizzazione a ws di trasmissione dati
boolean creaNuovoFile = false;   // abilita la creazione di un nuovo file presenze perchè il prec è stato trasmesso
boolean memLed2; //memoria per blink led2
bool scriviData = false;
bool monitorAttivo = false;  //dice che sono in monitor tx
short timer2minMem;  //memorizza valore di timer2min per ripristinare videata dopo monitor TX
boolean memPulsanteExtra = false;
boolean inProgr = false; //visualizza display solo dopo setup o programmazione
bool attesaEliminaInProgr = false;
int nuovoIndice;  //rappresenta l'indice dei vettori per orario classi dove inserire il nuovo record
String classeS = "   "; //classe come stringa. 
String classe[16]; //  vettore paralleo al successivo: questo indica la classe e l'altro l'orario di ingresso
word orarioIngrClassi[16][6]; //nella sesta colonna è memorizzata l'ora in minuti
uchar blockData[81] = {}; //contiene i 5 blocchi da programmare sulla card
uchar blockAddr; // select the operating block address 0 to 63

//***********************************************************************************
//NB scegliere una chiave segreta di 6 byte per la scrittura e lettura dei blocchi delle card
uint8_t keya[6] = {0x__, 0x__, 0x__, 0x__, 0x__, 0x__};
//uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; //chiave standard NON USARE!!!!!!
uchar *punt = &blockData[0];
char riga1Display[21] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
char riga2Display[21] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
char riga3Display[21] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 'P', ':', 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
char riga4Display[21] = {'0', 0x20, ':', '0', 0x20, ':', '0', 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 'T', ':'};

File dataFilePr, dataFileOr, dataFileM, dataFileData, dataEeprom; //file delle presenze e file orario ingresso classi
File dataFileClasse; //file che contiene i valori delle nuove classi relative alle matricole
boolean fileOK = false;
char timeDateStringa[11];
char timeStringa[5]; 
char timeStringaXMD5[7]; 
char matr[6];
String matrS = "     ";  //per aggiornamento classe
bool aggiornaClasseAttivo = false; // da settare tramite interfaccia web e aggiugengere a setup
int primaMatrFile; //prima matricola significativa presente ad inizio file aggclass.txt
String stGiorno_mese;   
String stMese;
String stAnno;
boolean admin = false;

//***********************************************************************************
//NB scegliere una password per l'accesso tramite browser alle impostazioni di sistema 
String password = "_________";
String passw, istituto;
String selettore;  //seleziona se setup o programmazione ingresso classi
String md5String;
String ipWebServer, dirWebServer, dir2WebServer, dir3WebServer;
char ipWebServerC[22]; //nel costruttore del client occore un array di caratteri non una Stringa
long time, time1; //per funzione millis()
long attesaInProgr; //timer per attesa di 10 sec per attesa logout
boolean okMelody, ritardoMelody, nota2;
byte maskRegAbil;
byte maskOrarioAbil;
bool callSetMaskOrarioAbil = true;
bool erroreRicezioneFileClassi;
byte CR;
EthernetServer server = EthernetServer(80);
EthernetClient clienth, client;
bool success;
bool flip = false; //per blink se sistema non attivo
String selFile;
boolean fileTrasmesso;
bool rit30sec;  //serve per ritardare di 30 sec la trasmissione a web server
byte minTolti;  //minuti da sottrarre a minuti inizio lezione per non trasmettere
byte contatoreXLed = 0;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  
uint8_t uidMem[] = { 0, 0, 0, 0, 0, 0, 0 };
uint8_t uidMemPrec[] = { 0, 0, 0, 0, 0, 0, 0 };
uint8_t uidLength;                        
uint32_t versiondata;

void setup()
{ wdt_disable();
  pinMode(chipSelectEth, OUTPUT);  //selezione ethernet
  pinMode(chipSelectSD, OUTPUT);  //selezione scheda SD
  pinMode(chipSelectRfid, OUTPUT);  //selezione scheda SD
  pinMode(pulsanteTest, INPUT); 
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);
  pinMode(capacitor, INPUT);
  pinMode(53, OUTPUT);   
  pinMode(A1, INPUT); // segnale per ritardare la trasmissione di uno dei due totem di 30 sec rispetto allo scadere del minuto
  pinMode(A2, INPUT); // segnale per totem in rete lan (cioè può trasmettere fino alle 8.9) oppure in internet (trasmette fino alle 8.5)
  // se segnale 1 -> rete lan; se segnale 0 -> internet; senza ponticello a massa si comporta da rete lan
  digitalWrite(A1, HIGH); 
  digitalWrite(A2, HIGH); 
  digitalWrite(chipSelectEth, HIGH); 
  digitalWrite(chipSelectSD, HIGH); 
  digitalWrite(chipSelectRfid, HIGH); 
  Serial.begin (9600);
  Serial.println("versione Rfid522readerfin25");
  if (digitalRead(capacitor) == HIGH) coldStart = false; 
  else coldStart = true;

  SPI.begin ();
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.setDataMode(SPI_MODE0);
  lcd.begin(20, 4);
  lcd.setCursor(0, 0);
  lcd.print("Rfid522readerfin25");
  
  rfid.init();


  //SD///////////////////////////////////////////////
  digitalWrite(chipSelectEth, HIGH); 
  digitalWrite(chipSelectSD, LOW); 

  
  if (!SD.begin(chipSelectSD))
  {  delay(500);  
    if (!SD.begin(chipSelectSD))
      {lcd.setCursor(0, 1);
      lcd.print("  Errore scheda SD  ");
      while (-1) {} }
  }
  lcd.setCursor(0, 2);
  lcd.print("Scheda SD  OK");
  delay(1000);
  if (digitalRead(pulsanteTest) == HIGH) loadSD2Eeprom();
  if (EEPROM.read(a_1byteIP) != 0 && EEPROM.read(a_1byteIP) != 255 && digitalRead(pulsanteTest) == LOW)
  { for (int i = 0; i < 4; i++)
      ipReset[i] = EEPROM.read(a_1byteIP + i);
  }
  if (EEPROM.read(a_1bytegateway) != 0 && EEPROM.read(a_1bytegateway) != 255 && digitalRead(pulsanteTest) == LOW)
  { for (int i = 0; i < 4; i++)
      gateway[i] = EEPROM.read(a_1bytegateway + i);
  }

  Ethernet.begin(mac, ipReset, dnsq, gateway);//, gateway, subnet);
  server.begin();
  delay(1000);

  leggiRTC();
  stGiorno_mese = (String)giorno_mese;
  stMese = (String)mese;
  stAnno = (String)anno;
  String stData = stGiorno_mese + stMese + stAnno;

  dataFileData = SD.open("datapres" , FILE_WRITE); //file che contiene solo la data giornaliera per gestire i 2 file presenze: cancellarli se giorno diverso
  if (dataFileData.size() > 0)
  { dataFileData.seek(0);
    String dataF = dataFileData.readStringUntil(';');
    if (!stData.equals(dataF)) 
    { 
      SD.remove("presenze");                    
      SD.remove("presmem");
      scriviData = true;
    }
  }
  else scriviData = true;
  if (scriviData)   
  { char chData[8];
    int indice = stData.length();
    for (int i = 0; i < indice; i++) chData[i] = stData[i];
    chData[indice] = ';';    
    dataFileData.seek(0);
    dataFileData.print(chData);
  }
  dataFileData.close();

  dataFilePr = SD.open("presenze", FILE_WRITE);  
  if (dataFilePr) fileOK = true;
  else {
    lcd.setCursor(0, 1);
    lcd.print("errore apertura file");
  }

  dataFileM = SD.open("presmem", FILE_WRITE);
  if (dataFileM.size() == 0)
  {
    dataFileM.seek(0);
    dataFileM.print(0);   
    dataFileM.print(';');
  }
  else
  {
    dataFileM.seek(0);
    puntFilePres =  dataFileM.readStringUntil(';').toInt();   
  }

  if (fileOK && dataFilePr.size() > puntFilePres)   
    newRecSD = true;  
  }

  dataFilePr.close();
  dataFileM.close();

  caricaSetup();
  if (aggiornaClasseAttivo)
    if (verificaFileAggClass() != 0)
    { lcd.setCursor(0, 2);      lcd.print("file aggclass ERRATO");
      lcd.setCursor(0, 3);      lcd.print("      errore matr.");  lcd.print(verificaFileAggClass());
      while (-1) {}
    }  

  caricaOrarioIngrClassi();
  maskOrarioAbil = setMaskOrarioAbil(ora);  
  maxOrarioIngresso = orarioLezioni[0][2] + mRitIngresso + minutiExtra; 
  leggiRTC(); 
  if (coldStart)
  { digitalWrite(chipSelectSD, HIGH); 
    digitalWrite(chipSelectRfid, HIGH); 
    digitalWrite(chipSelectEth, LOW); 
    lcd.setCursor(0, 0);
    lcd.print("                    ");
    setWDT(_8secIntRes);   
    aggiornaOra();
  }

  calcolaProgrGiorni();         
  oraInMinuti = ora * 60 + minuti;
  if (oraInMinuti >= maxOrarioIngresso) orarioIngressoRitardo = true; else orarioIngressoRitardo = false;
  delay(500); 
  dataDisplay = calcolaDataXDisplay();
  lcd.setCursor(0, 3);
  lcd.print("                    ");
  visualizzaDisplay();
  timeDateToString();
  for (int i = 0; i < ipWebServer.length(); i++) ipWebServerC[i] = ipWebServer[i];

  if (disattivaPrimaOra && (((oraInMinuti >= orarioLezioni[0][2] + mRitIngresso - 1) && (oraInMinuti <= orarioLezioni[0][2] + disattMin))
                            || ((oraInMinuti >= orarioLezioni[0][2] + durata + mRitIngresso - 1) && (oraInMinuti <= orarioLezioni[0][2] + durata + disattMin))
                            || ((oraInMinuti >= orarioLezioni[0][2] + 2 * durata + mRitIngresso - 1) && (oraInMinuti <= orarioLezioni[0][2] + 2 * durata + disattMin))))   
                            primaOra = true; else primaOra = false;

  if (digitalRead(A1) == LOW) rit30sec = true;
  if (digitalRead(A2) == LOW) minTolti = 5; else minTolti = 1;
  if (coldStart) setup2();
}

void setup2()
{
  lcd.setCursor(0, 0);
  lcd.print("                    ");

    digitalWrite(chipSelectSD, HIGH); 
    digitalWrite(chipSelectRfid, HIGH); 
    digitalWrite(chipSelectEth, LOW); 
    wdt_reset();
    setWDT(_8secIntRes);  
    erroreRicezioneFileClassi= true;
    if (selFile == "ws") 
       { richiediDatiWeb(1);   
        lcd.setCursor(0, 2);
        if (!erroreRicezioneFileClassi) 
           lcd.print("ricevuto file classi");  else lcd.print("file classi no ricev");
       }
  wdt_reset();
  setWDT(_8secIntRes);   

  startMelody();
  lcd.setCursor(17, 3);
  lcd.print("   ");
  digitalWrite(chipSelectSD, HIGH); 
  digitalWrite(chipSelectEth, HIGH); 
  digitalWrite(chipSelectRfid, HIGH); 
}

void loop()
{  
  wdt_reset();
  setWDT(_4secRes); 
 
  if (nextDay)
  { nextDay = false;

  delay(3000);
    wdt_reset();
    setWDT(_2secRes); 
    delay(3000);
  }

  if (callSetMaskOrarioAbil) maskOrarioAbil = setMaskOrarioAbil(ora);
  accendiLed();
  if (!nonAttivo)
    if (!memLed2)
    { contatoreXLed++;
      digitalWrite(led2, HIGH);
       if (contatoreXLed >= 5) {memLed2 = true; contatoreXLed = 0; }
    }
    else {
      digitalWrite(led2, LOW);
      memLed2 = false;
    }
  else digitalWrite(led2, HIGH);

  digitalWrite(chipSelectSD, HIGH); 
  digitalWrite(chipSelectRfid, HIGH); 
  digitalWrite(chipSelectEth, LOW); 
  IPAddress serverIPAddress = Ethernet.localIP();
 if (!(client  && admin && ((!leggiSetup && !setupOK && selettore == "set") || (!leggiProgr && !progrOK && selettore == "prg"))))

   client = server.available();

  if (digitalRead(pulsanteTest) == HIGH && !memPulsanteExtra && !orarioIngressoRitardo) 
  { minutiExtra++;
    memPulsanteExtra = true;
    maxOrarioIngresso = orarioLezioni[0][2] + mRitIngresso + minutiExtra;
  }
  if (digitalRead(pulsanteTest) == LOW) memPulsanteExtra = false;
  aggiornaOraDisplay();  
  if (monitorAttivo && timer2minMem < timer2min) 
  {
    monitorAttivo = false;
    visualizzaDisplay();
  }
      if (oraInMinuti < orarioLezioni[0][2] -15)   
      { nonAttivo = true;
        if (!inProgr)
          if (!flip)
           { visualizzaDis(7, 55); lcd.display(); time = millis(); flip = true;}
       
          else if(millis() - time > 650) {lcd.noDisplay(); delay(150); flip = false;}
      }    
     else
    { lcd.display();  
      nonAttivo = false;
  digitalWrite(chipSelectSD, HIGH); 
  digitalWrite(chipSelectEth, HIGH);  
  digitalWrite(chipSelectRfid, LOW); 
      if (leggiCard())
     {
      tone(buz, NOTE_C5);    
      time = millis();
      if (!ritardo && !cardRipassata)
      {
        okMelody = true;
      }

      visualizzaDatiSuDisplay();

      char cod;
      if (cardRipassata) cod = 'U'; else if (ritardo) cod = 'R'; else cod = 'I';
      { 
        scriviMatricolaSuFile(cod, timeStringa);   
        newRecSD = true;      
      }
      if (ritardo)
      { lcd.setCursor(13, 2);
        lcd.print("RITARDO");
        ritardoMelody = true;
      }
      if (cardRipassata)
      { lcd.setCursor(13, 2);
        lcd.print("USCITA ");
        ritardoMelody = true;
      }

      cardRipassata = false;
      ritardo = false;
      timer2min = 0;  
     } 
    if (okMelody || ritardoMelody)
    {
      if (millis() - time  > 40)   //suona la seconda nota
      { 
        noTone(buz);
        okMelody = false;
        if (ritardoMelody)
        {
          tone(buz, NOTE_A4);
          ritardoMelody = false;
          time1 = millis();
          nota2 = true;
        }
      }
    }
    if (nota2 && (millis() - time1  > 80)) {
      noTone(buz);
      nota2 = false;
    }
    wdt_reset();
    if ((!rit30sec && (contatoreNTx < 2) && trasmettiWS) || (rit30sec && (contatoreNTx < 2) && trasmettiWS && secondi > 29)              
          || (digitalRead(pulsanteTest) == HIGH && orarioIngressoRitardo))
    {
      contatoreNTx++;
      noTone(buz);  nota2 = false;
      richiediDatiWeb(0);  //richiesta autorizzazione a trasmettere
    }
    wdt_reset();
    if (selFile == "ws" && oraInMinuti == 519 && secondi == 1) 
          {       
          erroreRicezioneFileClassi= true;
          richiediDatiWeb(1);   
          lcd.setCursor(0, 2);
          if (!erroreRicezioneFileClassi) 
              lcd.print("ricevuto file classi");  else lcd.print("file classi no ricev");
          }
    
    setWDT(_4secRes);

  } 

  digitalWrite(chipSelectSD, HIGH); 
  digitalWrite(chipSelectRfid, HIGH); 
  digitalWrite(chipSelectEth, LOW); 
  
  if (inProgr) trasmettiWS = false;      
  if (client || admin)
  {
    inProgr = true;
    spegniLed();
    visualizzaWait("In programmazione...", monitorAttivo);
  }
  else
  { if (inProgr)
    {
      accendiLed();
      visualizzaDisplay();
      inProgr = false;
    }
  }
  if (client && !admin)
  {
    while (client.connected())
    {                                                
      if (client.available())
      {                                            
        if (client.find("sel="))
        {                                           
          selettore = client.readStringUntil('&');
        }
        else visualizzaPaginaLogin(client);
        if (client.find("passwd="))
        {                                          
          passw = "";
          passw = client.readStringUntil(' ');

          if (passw == password)
            {  
              if (selettore == "fil" && selFile == "ws")
                  {visualizzaPaginaLogin(client);
                   erroreRicezioneFileClassi= true;
                   richiediDatiWeb(1);   
                   if (!erroreRicezioneFileClassi)                     
                       client.println("file orario classi ricevuto");                                              
                   else client.println("errore ric. file orario classi");
                   client.stop();
                   break;
                  }
              else{  
                  admin = true;
                  }
            }
            else {visualizzaPaginaLogin(client); client.println( "password errata");}
          }
          break;        
      }
    }
   if (!admin) {client.stop(); }
  }

  if (client  && admin && !leggiSetup && !setupOK && selettore == "set")
  {
    visualizzaPaginaSetup(client);
    leggiSetup = true;
    client.stop();
  }

  if (client && admin && !leggiProgr && !progrOK && selettore == "prg")
  {  
    nuovoIndice = visualizzaPaginaProgr(client);
    leggiProgr = true;        
    client.stop();
  }

  if (client && admin && leggiSetup)
  {
    leggiSetupSalva(client);
    leggiSetup = false;
    setupOK = true;
    visualizzaChiudiSetup(client);
    attesaEliminaInProgr = true; 
    attesaInProgr = millis();
    client.stop();
  }

  if (client && admin && leggiProgr)
  { 
    leggiProgrClassi(client, nuovoIndice, 0);  
    leggiProgr = false;
    progrOK = true;
    visualizzaChiudiSetup(client);
    attesaEliminaInProgr = true; 
    attesaInProgr = millis();
    client.stop();
  }
 
  if ((client && admin && (setupOK || progrOK)) || (millis() - attesaInProgr > 10000) && attesaEliminaInProgr)
  { attesaEliminaInProgr = false;
    if (client.find("lgt") || (millis() - attesaInProgr > 10000))
    {
      setupOK = false;
      progrOK = false;
      admin = false;
      client.stop();
      dataDisplay = calcolaDataXDisplay(); 
    }
    else if (progrOK) 
       { progrOK = false;           

       }
        else if (setupOK)
           { setupOK = false;

           }
  }

rfid.halt();     
}  
// ISR interrupt routine per aggiornamento orologio
//_________________________________________________________________________________________
void clock()
{
  sem = true;
  if (secondi >= 59)
  { secondi = 0;
    if (minuti >= 59)
    { minuti = 0;
      if (ora >= 23)
      {
        nextDay = true;
      }
      else {
        ora++;
        callSetMaskOrarioAbil = true;
      }
    }
    else 
    {
      minuti++;    
    }
    oraInMinuti = ora * 60 + minuti;
    if (oraInMinuti >= maxOrarioIngresso) orarioIngressoRitardo = true; else orarioIngressoRitardo = false;

    if (!inProgr && (newRecSD || !fileTrasmesso))
      if ((oraInMinuti >= (orarioLezioni[0][2] - 15)) && (oraInMinuti <= (orarioLezioni[0][2] - minTolti)))
       { unaTx = true;  
         contatoreNTx = 0;
         trasmettiWS = true; 
       }
       else if ((oraInMinuti >= (orarioLezioni[0][2] + mRitTx)) && (oraInMinuti <= (orarioLezioni[0][2] + 360)))
    { 
      contatoreNTx = 0;
      trasmettiWS = true; 
    }
    if (disattivaPrimaOra && (((oraInMinuti >= orarioLezioni[0][2] + mRitIngresso - 1) && (oraInMinuti <= orarioLezioni[0][2] + disattMin))
                            || ((oraInMinuti >= orarioLezioni[0][2] + durata + mRitIngresso - 1) && (oraInMinuti <= orarioLezioni[0][2] + durata + disattMin))
                            || ((oraInMinuti >= orarioLezioni[0][2] + 2 * durata + mRitIngresso - 1) && (oraInMinuti <= orarioLezioni[0][2] + 2 * durata + disattMin))))   
                            primaOra = true; else primaOra = false;

    timeDateToString();
    timer2min++;
    if (timer2min == 2)
    {
      visualizzaDisplay();

      for (int i=0; i<4;i++) uidMem[i] = 0;  //altrimenti l'ultimo entrato non può uscire
    }
  }
  else {
    secondi++;
  }
}

/*___________________________________________________________________________
leggiCard
Funzione per la lettura di rfid card
*/
bool leggiCard()
{ 
  bool faseOk = false;
  bool cardDiversa = false;
  if (rfid.isCard())
   { 
   rfid.readCardSerial();
    }    
    RC_size = rfid.MFRC522SelectTag(rfid.serNum);  //non togliere mai

// Card Reader

    blockAddr = 15; 
    status = rfid.auth(PICC_AUTHENT1A, blockAddr, keya , rfid.serNum); 
  
    if (status == MI_OK)
    {  blockAddr = 12;
       for (int i=0; i<3; i++)       //legge i primi 3 blocchi: cognome, nome, etc.
       { 
 // Read data
          status = rfid.read(blockAddr, punt + i*16 );
          if (status == MI_OK)
          {    faseOk = true;                 
          } 
          blockAddr++;
        } 
    }  //fine lettura primi 3 blocchi

    if (faseOk)
    { faseOk = false;
       for (int i=0; i<4;i++)        
          if (uidMem[i] != rfid.serNum[i]) {cardDiversa = true; break; }  
       if (!cardDiversa) return false;   
       blockAddr = 19; 
       status = rfid.auth(PICC_AUTHENT1A, blockAddr, keya , rfid.serNum); 
       if (status == MI_OK)
       {  blockAddr = 16;
          for (int i=0; i<2; i++)       //legge il quarto e quinto blocco: orario settimanale individuale
          {   
     // Read data
             status = rfid.read(blockAddr, punt + (3+i)*16 );
             if (status == MI_OK)
             {  faseOk = true;    
             }  
           blockAddr++;  
          }
       }  
    } 
      //carica la classe
    if (faseOk)
    {faseOk = false;
          for (int j = 43; j < 46; j++) 
          { riga3Display[j - 43] = *(punt + j);
            classeS.setCharAt(j - 43, *(punt + j));
          }     
          if (((blockData[iRegistro] & maskRegAbil & maskOrarioAbil) != 0) || maskRegAbil == 0)
          {
              if (blockData[iProgrGiorni] != progrGiorni)  
              { 
                if (orarioIngressoRitardo)
                { 
                  ritardo = verificaSeRitardo();
                }
                else ritardo = false;
    
                if (ritardo) blockData[iProgrRitardi] = blockData[iProgrRitardi] + 1;
                blockData[iProgrPresenze] = blockData[iProgrPresenze] + 1;
                blockData[iProgrGiorni] = progrGiorni;
                cardRipassata = false;
              }
              else
              {
              byte minimoUscita = max (blockData[iFuorisede], mRitIngresso);
              if (oraInMinuti > orarioLezioni[0][2] + minimoUscita +1 + minutiExtra)
                 {blockData[iProgrGiorni] = progrGiorni - 1; 
                 cardRipassata = true;
                 ritardo = false;
                 }
               else    
                 {lcd.setCursor(0, 0); lcd.print("**   ATTENZIONE   **" );
                  lcd.setCursor(0, 1); lcd.print("  HAI GIA' TIMBRATO ");
                  lcd.setCursor(0, 2); lcd.print("                    ");
                   return false;           
                 }
              }
            if (!disattivaPrimaOra || !primaOra || !ritardo)
            {
                blockAddr = 17;
                status = rfid.write(blockAddr, punt + 64);   
                if (status != MI_OK)
                {
                   status = rfid.write(blockAddr, punt + 64);    
                   if (status != MI_OK)
                      {lcd.setCursor(0,0);
                       lcd.print("**ERROR WRITE CARD**");}
                   else faseOk = true;
                }
                else faseOk = true;  
            }
            else {visualizzaDis(ora, orarioLezioni[0][1] + disattMin);
                  timer2min = 0;
                 }         
          if (aggiornaClasseAttivo)
          if (aggiornaClasse())
          {
            for (int j = 43; j < 46; j++) 
              riga3Display[j - 43] = *(punt + j);
    
            blockAddr = 15; 
            status = rfid.auth(PICC_AUTHENT1A, blockAddr, keya , rfid.serNum);          
    
            if (status == MI_OK)
            {
               blockAddr = blockAddr - 1;  
               status = rfid.write(blockAddr, punt + 32 );  
               if (status == MI_OK)
               {
               }
             else
              {             
                Serial.println ("errore aggiorn. classe");
              }
            }
          }      
    }  
 
  } 
       
    if (faseOk)
      { spegniLed();
       for (int i=0; i<4;i++) 
          {
            uidMem[i] = rfid.serNum[i];  
          }
        return true;
      }
      else 
      {  for (int i=0; i<4;i++) uidMemPrec[i] = 0;  
        return false;
      }
}

/*___________________________________________________________________________
aggiornaOraDisplay
aggiorna la visualizzazione dell'ora sul display
*/
void aggiornaOraDisplay()
{
  if (sem)
  {
    lcd.setCursor(0, 3);
    lcd.print(riga4Display);
    if (minutiExtra != 0 && !orarioIngressoRitardo)
    { lcd.setCursor(13, 3);
      lcd.print("+");
      lcd.print(minutiExtra);
    }

    if (ora < 10)
    { lcd.setCursor(1, 3);
      lcd.print(ora);
    }
    else
    { lcd.setCursor(0, 3);
      lcd.print(ora);
    }
    if (minuti < 10)
    { lcd.setCursor(4, 3);
      lcd.print(minuti);
    }
    else
    { lcd.setCursor(3, 3);
      lcd.print(minuti);
    }
    if (secondi < 10)
    { lcd.setCursor(7, 3);
      lcd.print(secondi);
    }
    else
    { lcd.setCursor(6, 3);
      lcd.print(secondi);
    }
    lcd.setCursor(10, 3);
    lcd.print(gSett);
    sem = false;
  }
}

/*___________________________________________________________________________
visualizzaDatiSuDisplay
visualizza su display i dati relativi all'ultima card letta
*/
void visualizzaDatiSuDisplay()
{
  lcd.setCursor(0, 0);
  lcd.print("                    ");
  for (int j = 0; j < 20; j++)
  {
    riga1Display[j] = *(punt + j); //carica cognome
  }
  lcd.setCursor(0, 0);
  lcd.print(riga1Display);
  lcd.setCursor(0, 1);
  lcd.print("                    ");  
  for (int j = 20; j < 38; j++)
  {
    riga2Display[j - 20] = *(punt + j); //carica nome
  }

  lcd.setCursor(0, 1);
  lcd.print(riga2Display);
  progrPresenze = blockData[iProgrPresenze];
  progrRitardi = blockData[iProgrRitardi];
  totPresenzeGiorno = totPresenzeGiorno + 1;
 
  lcd.setCursor(0, 2);
  lcd.print(riga3Display);
  if (ritardo)
  { lcd.setCursor(7, 2);
    lcd.print("R");
    lcd.setCursor(9, 2);
    lcd.print(progrRitardi);
  }
  else
  {
    lcd.setCursor(9, 2);
    lcd.print(progrPresenze);
  }
  lcd.setCursor(17, 3);   
  lcd.print(totPresenzeGiorno);
}

/*___________________________________________________________________________
scriviMatricolaSuFile
scrive su file 'presenze' su SD i dati relativi alla card letta: matricola, codice, ora
*/
void scriviMatricolaSuFile(char codice, char timep[])
{ wdt_reset();
  digitalWrite(chipSelectEth, HIGH); 
  digitalWrite(chipSelectSD, LOW); 
  byte ii = 0 ;
  do
  { dataFilePr = SD.open("presenze", FILE_WRITE);
    delay(20);
    ii++;
  }
  while (!dataFilePr && ii < 4);
  if (dataFilePr)
  { 
    for (int j = 38; j < 43; j++) 
    {
      matr[j - 38] = *(punt + j);
    }
    dataFilePr.print(matr);
    dataFilePr.print(codice);
    dataFilePr.print(timep);
    dataFilePr.println(';');
    dataFilePr.flush();
    dataFilePr.close();
    digitalWrite(chipSelectSD, HIGH); 
    digitalWrite(chipSelectRfid, HIGH); 
    digitalWrite(chipSelectEth, LOW); 
  }
  else
  {
    lcd.setCursor(0, 2); lcd.print(' RIPASSA LA CARD '); 
    wdt_reset();
    setWDT(_2secRes); 
    delay(3000);
  }
}

/*___________________________________________________________________________
visualizzaPaginaLogin
visualizza su browser form di login
*/
void visualizzaPaginaLogin(EthernetClient client)
{
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println();
  client.println(F("<HTML>"));
  client.println(F("<BODY>"));
  client.println(F("<H1> I S I S   \" D I   M A G G I O \"</H1>"));
  client.println(F("<br>"));
  client.println(F("<H2> LETTORE  RFID CARD </H2>"));
  client.println(F("<br>"));
  client.println(F("<H5>"));
  client.println(F("<br><br>"));
  client.println(F("<form name=\"admin\" METHOD= \"GET\">"));
  client.println(F("Per entrare nella programmazione effettua login"));
  client.println(F("<br><br>"));
  client.println(F("<INPUT TYPE=\"Radio\" Name=\"sel\" Value=\"prg\" checked=\"yes\">programmazione orario ingresso classi<br>"));
  client.println(F("<INPUT TYPE=\"Radio\" Name=\"sel\" Value=\"set\">setup<br>"));
  if (selFile == "ws")
      client.println(F("<INPUT TYPE=\"Radio\" Name=\"sel\" Value=\"fil\">leggi file ingresso classi da web server<br>"));
  client.println(F("<br>"));
  client.println(F("password<input type=\"password\" name=\"passwd\"><br>"));
  client.println(F("<br>"));
  client.println(F("<input type=\"submit\" value=\"Login\" >"));
  client.println(F("<br>"));
  client.println(F("</form>"));
  client.println(F("</BODY>"));
  client.println(F("</HTML>"));
}

/*___________________________________________________________________________
visualizzaPaginaSetup
visualizza su browser form di setup del sistema
*/
void visualizzaPaginaSetup(EthernetClient client)
{
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println();
  client.println(F("<HTML>"));
  client.println(F("<BODY>"));
  client.println(F("<H2> I S I S   \" D I   M A G G I O \""));
  client.println(F("<br><br>"));
  client.println(F("LETTORE  RFID CARD </H2>"));
  client.println(F("<br>"));
  client.println(F("<H3> SETUP </H3>"));
  client.println(F("<H5>"));
  client.println(F("<form name=\"admin\" METHOD= \"GET\">"));
  client.println(F("<br>"));
  client.println(F("<TABLE>"));
  client.println(F("<TR><TD>Nome ISTITUTO</TD><TD>"));
  client.print(istituto);
  client.println(F("</TD><TD><input type=\"text\"  name=\"ist\" value=\"*\"></TD></TR></TABLE>"));
  client.println(F("<br>"));
  client.println(F("<TABLE>"));
  client.println(F("<TR><TD>Data</TD><TD>"));
  client.print(gSett); client.print(" ");
  client.print(giorno_mese); client.print("/");
  client.print(mese); client.print("/");
  client.print(anno);
  client.println(F("</TD><TD><select name =\"gse\">"));
  client.println(F("<option> * </option>"));
  client.println(F("<option> LUN </option>"));
  client.println(F("<option> MAR </option>"));
  client.println(F("<option> MER </option>"));
  client.println(F("<option> GIO </option>"));
  client.println(F("<option> VEN </option>"));
  client.println(F("<option> SAB </option>"));
  client.println(F("<option> DOM </option>"));
  client.println(F("</select>"));
  client.println(F("</TD><TD><input type=\"text\"  name=\"gio\" maxlength=\"2\" size=\"2\" value=\"*\">"));
  client.print(F("/<input type=\"text\"  name=\"mes\" maxlength=\"2\" size=\"2\" value=\"*\">"));
  client.print(F("/<input type=\"text\" maxlength=\"2\" size=\"2\" name=\"ann\" value=\"*\"></TD></TR>"));
  client.println(F("<TR><TD>Ora</TD><TD>"));
  client.print(ora); client.print(":");
  client.print(minuti); client.print(":");
  client.print(secondi);
  client.println(F("</TD><TD><input type=\"text\"  name=\"ora\" maxlength=\"2\" size=\"2\" value=\"*\">"));
  client.print(F(":<input type=\"text\"  name=\"min\" maxlength=\"2\" size=\"2\" value=\"*\">"));
  client.print(F(":<input type=\"text\"  name=\"sec\" maxlength=\"2\" size=\"2\" value=\"*\"></TD></TR></TABLE>"));
  client.println(F("<br>"));
  client.println(F("Ora ingresso"));
  client.print(F("<TABLE>"));
  client.print("<TR><TD>1^ora mattino:</TD><TD>");
  client.print(orarioLezioni[0][0]); client.print(":"); client.print(orarioLezioni[0][1]);
  client.print(F("</TD><TD><input type=\"text\"  name=\"prHM\" maxlength=\"2\" size=\"2\" value=\"*\"></TD>"));
  client.print(F("<TD><input type=\"text\"  name=\"prHMmin\" maxlength=\"2\" size=\"2\" value=\"*\"></TD>"));
  client.print(F("<TD>Durata periodo lezione:</TD><TD>"));
  client.print(durata);
  client.print(F("</TD><TD><input type=\"text\"  name=\"durataM\" maxlength=\"2\" size=\"2\" value=\"*\"></TD><TD>"));
  client.println("min");
  client.print(F("</TD></TR></TABLE>"));
  client.println(F("<br>"));
  client.print(F("Margine ritardo ingresso 1^ ora: ")); client.print(mRitIngresso);
  client.print(F("<input type=\"text\"  name=\"mri\" maxlength=\"2\" size=\"2\" value=\"*\">")); client.println("min");
  client.println(F("<br><br>"));
  client.print(F("Ritardo prima trasmissione: ")); client.print(mRitTx);
  client.print(F("<input type=\"text\"  name=\"mrtx\" maxlength=\"2\" size=\"2\" value=\"*\">")); client.println("min");
  client.println(F("<br><br>"));
  client.print(F("indirizzo web server: ")); client.print(ipWebServer);
  client.print(F("<input type=\"text\"  name=\"ipW\" size=\"40\" value=\"*\">"));
  client.println(F("<br><br>"));
  client.print(F("Path web server richiesta TX: ")); client.print(dirWebServer);
  client.print(F("<input type=\"text\"  name=\"dirW\" size=\"50\" value=\"*\">"));
  client.println(F("<br><br>"));
  client.print(F("Path web server file PHP TX: ")); client.print(dir2WebServer);
  client.print(F("<input type=\"text\"  name=\"dir2W\" size=\"50\" value=\"*\">"));
  client.println(F("<br><br>"));
  if (selFile == "br")
  {client.println(F("<INPUT TYPE=\"Radio\" Name=\"fil\" Value=\"br\"checked=\"yes\">programmazione orario ingresso classi da browser<br>"));
  client.println(F("<INPUT TYPE=\"Radio\" Name=\"fil\" Value=\"ws\">programmazione orario ingresso classi da registro<br><br>"));
  }
  else
    if (selFile == "ws")
     {client.println(F("<INPUT TYPE=\"Radio\" Name=\"fil\" Value=\"br\">programmazione orario ingresso classi da browser<br>"));
      client.println(F("<INPUT TYPE=\"Radio\" Name=\"fil\" Value=\"ws\"checked=\"yes\">programmazione orario ingresso classi da registro<br><br>"));
  
      client.print(F("Path web server file orario ingresso classi: ")); client.print(dir3WebServer);
      client.print(F("<input type=\"text\"  name=\"dir3W\" size=\"50\" value=\"*\">"));
      client.println(F("<br><br>"));
     }
  client.print(F("indirizzo ip: "));
  client.print(ipReset[0]); client.print("."); client.print(ipReset[1]); client.print(".");
  client.print(ipReset[2]); client.print("."); client.print(ipReset[3]);
  client.print(F("<input type=\"text\"  name=\"ipR\" size=\"15\" value=\"*\">"));
  client.println(F("<br><br>"));
  client.print(F("indirizzo gateway: "));
  client.print(gateway[0]); client.print("."); client.print(gateway[1]); client.print(".");
  client.print(gateway[2]); client.print("."); client.print(gateway[3]);
  client.print(F("<input type=\"text\"  name=\"ipG\" size=\"15\" value=\"*\">"));
  client.println(F("<br><br>"));
  client.print("Maschera registri abilitati:  "); client.print(maskRegAbil, BIN);
  client.print(F("<input type=\"text\"  name=\"mAR\" size=\"15\" value=\"*\">"));  //maschera abilitazione registri
  client.println(F("<br><br>"));
  client.print(F("Aggiornamento classi da file: "));
  if (aggiornaClasseAttivo) client.print("attivo       ");
  else client.print(F("non attivo  "));
  client.print(F("<select name = \"acl\">"));
  client.println(F("<option> * </option>"));
  if (aggiornaClasseAttivo)
    client.println(F("<option> disattiva </option>"));
  else client.println(F("<option> attiva </option>"));
  client.println(F("</select>"));
  client.println(F("<br><br>"));
  client.print(F("Minuti disattivazione timbratura ritardatari: ")); client.print(disattMin);
  client.print(F("<input type=\"text\"  name=\"dmr\" maxlength=\"2\" size=\"2\" value=\"*\">")); client.println("min");
  client.println(F("<br><br>"));
  client.print(F("Stringa per MD5: ")); for (int i=0; i<32;i++) client.print(char (EEPROM.read(a_md5 + i)));
  client.print(F("<input type=\"text\"  name=\"md5\" maxlength=\"32\" size=\"32\" value=\"*\">")); 
  client.println(F("<br><br>"));
  client.print(F("<input type=\"hidden\"  name=\"hid\" value=\"h\">")); 
  if (disattivaPrimaOra)
    client.print(F("<INPUT TYPE=\"checkbox\" Name=\"dpo\" Value=\"dpo\" checked=\"yes\">Disattiva timbratura ritardatari prima ora"));
  else
    client.print(F("<INPUT TYPE=\"checkbox\" Name=\"dpo\" Value=\"dpo\">Disattiva timbratura ritardatari prima ora"));
  client.println(F("<br><br>"));
  client.print(F("<input type=\"submit\" value=\"salva\" >"));
  client.println(F("   N.B. in caso di modifiche al setup il sistema si riavvia. Chiudere il browser"));
  client.println(F("</form>"));
  client.println(F("</BODY>"));
  client.println(F("</HTML>"));
}

/*___________________________________________________________________________
leggiSetupSalva
routine per il salvataggio in eeprom dei dati impostati nel setup 
*/
void leggiSetupSalva(EthernetClient client)
{
  String tempF;
  boolean setOrologio = false;
  bool setOrarioLezioni = false;  
  bool setRestart = false; 
  if (client.find("ist="))
  { tempF = client.readStringUntil('&');
    if (tempF  != "*")
    { istituto = tempF;
      for (int i = a_istituto; i < istituto.length(); i++)
      {
        EEPROM.write(i, istituto[i - a_istituto]);
      }
      EEPROM.write(a_istituto + istituto.length(), 255); 
    }
  }

  if (client.find("gse="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { gSett = tempF;
      setOrologio = true;
    }
  }

  if (client.find("gio="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { giorno_mese = tempF.toInt();
      setOrologio = true;
    }
  }
  if (client.find("mes="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { mese = tempF.toInt();
      setOrologio = true;
    }
  }
  if (client.find("ann="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { anno = tempF.toInt();
      setOrologio = true;
    }
  }
  if (client.find("ora="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { ora = tempF.toInt();
      setOrologio = true;
    }
  }
  if (client.find("min="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { minuti = tempF.toInt();
      setOrologio = true;
    }
  }
  if (client.find("sec="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { secondi = tempF.toInt();
      setOrologio = true;
    }
  }
  if (client.find("prHM="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*") {
      orarioLezioni[0][0] = tempF.toInt();
      setOrarioLezioni = true;
    }
  }
  if (client.find("prHMmin="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")  {
      orarioLezioni[0][1] = tempF.toInt();
      setOrarioLezioni = true;
    }
  }
  if (client.find("durataM="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")  {
      durata = tempF.toInt();
      setRestart = true;
      EEPROM.write(a_durata, durata);   
    }
  }  
  if (client.find("mri="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { setRestart = true;
      mRitIngresso = tempF.toInt();
      EEPROM.write(a_mRitIngresso, mRitIngresso);
    }
  }
  if (client.find("mrtx="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { setRestart = true;
      mRitTx = tempF.toInt();
      EEPROM.write(a_mRitTx, mRitTx);
    }
  }
  if (client.find("ipW="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { setRestart = true;
      ipWebServer = tempF;
      for (int i = a_ipWebServer; i < a_ipWebServer + ipWebServer.length(); i++)
      {
        EEPROM.write(i, ipWebServer[i - a_ipWebServer]);
      }
      EEPROM.write(a_ipWebServer + ipWebServer.length(), 255); //fine stringa
    }
  }
  if (client.find("dirW="))
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { setRestart = true;
      dirWebServer = tempF;
      dirWebServer.replace("%2F", "/");
      dirWebServer.replace("%3F", "?");
      dirWebServer.replace("%3D", "=");
    }
    for (int i = a_dirWebServer; i < a_dirWebServer + dirWebServer.length(); i++)
       EEPROM.write(i, dirWebServer[i - a_dirWebServer]);

    EEPROM.write(a_dirWebServer + dirWebServer.length(), 255); //fine stringa
  }
   if (client.find("dir2W="))
    { tempF = client.readStringUntil('&');
      if (tempF != "*")
      { setRestart = true;
        dir2WebServer = tempF;
        dir2WebServer.replace("%2F", "/");
        dir2WebServer.replace("%3F", "?");
        dir2WebServer.replace("%3D", "=");
      }
       for (int i = a_dir2WebServer; i < a_dir2WebServer + dir2WebServer.length(); i++)
         EEPROM.write(i, dir2WebServer[i - a_dir2WebServer]);

       EEPROM.write(a_dir2WebServer + dir2WebServer.length(), 255); //fine stringa
     }
 if (client.find("fil="))
  { tempF = client.readStringUntil('&');
    if (!tempF.equals(selFile))
    { setRestart = true;
      if (tempF == "br") EEPROM.write(a_selFile, 255); 
      else if (tempF == "ws") EEPROM.write(a_selFile, 0); 
    }
    if (tempF == "ws")
      {
        if (client.find("dir3W="))
          { tempF = client.readStringUntil('&');
            if (tempF != "*")
            { setRestart = true;
              dir3WebServer = tempF;
              dir3WebServer.replace("%2F", "/");
              dir3WebServer.replace("%3F", "?");
              dir3WebServer.replace("%3D", "=");
            }
    for (int i = a_dir3WebServer; i < a_dir3WebServer + dir3WebServer.length(); i++)
       EEPROM.write(i, dir3WebServer[i - a_dir3WebServer]);

    EEPROM.write(a_dir3WebServer + dir3WebServer.length(), 255); //fine stringa
         } 
    }
 }    
  if (client.find("ipR="))
  { char c = client.read();
    if ( c != '*')
    { setRestart = true;
      tempF = c + client.readStringUntil('.');
      ipReset[0] = tempF.toInt();
      ipReset[1] = client.readStringUntil('.').toInt();
      ipReset[2] = client.readStringUntil('.').toInt();
      ipReset[3] = client.readStringUntil(' ').toInt();

      for (int i = 0; i < 4; i++)
      {
        EEPROM.write(a_1byteIP + i, ipReset[i]);
      }
    }
  }
  if (client.find("ipG="))
  { char c = client.read();
    if ( c != '*')
    { setRestart = true;
      tempF = c + client.readStringUntil('.');
      gateway[0] = tempF.toInt();
      gateway[1] = client.readStringUntil('.').toInt();
      gateway[2] = client.readStringUntil('.').toInt();
      gateway[3] = client.readStringUntil(' ').toInt();
      for (int i = 0; i < 4; i++)
      {
        EEPROM.write(a_1bytegateway + i, gateway[i]);
      }
    }
    if (client.find("mAR="))
    {

      tempF = client.readStringUntil('&');
      if (tempF != "*")
      { setRestart = true;
        maskRegAbil = 0;
        int pot2 = 1;
        for (int i = 7; i >= 0; i--) {
          maskRegAbil = maskRegAbil + (byte (tempF.charAt(i)) - 0x30) * pot2;
          pot2 = pot2 * 2;
        }
        EEPROM.write(a_maskRegAbil, maskRegAbil);
      }
    }
    if (client.find("acl="))
    { tempF = client.readStringUntil('&');
      if (tempF != "*")
      { setRestart = true;
        if (tempF.equals("attiva")) {
          EEPROM.write(a_aggClass, 1);
          aggiornaClasseAttivo = true;
        }
        if (tempF.equals("disattiva")) {
          EEPROM.write(a_aggClass, 0);
          aggiornaClasseAttivo = false;
        }
      }
    }
  if (client.find("dmr="))   
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { setRestart = true;
      disattMin = tempF.toInt();
      EEPROM.write(a_disattMin, disattMin);
    }
  }    
   if (client.find("md5="))    //md5
  { tempF = client.readStringUntil('&');
    if (tempF != "*")
    { setRestart = true;
   for (int i =0; i<32; i++)   EEPROM.write(a_md5 + i, tempF[i]);
    }
  }    
    if (client.find("dpo=")) //ripetizione programmazione da file
    {
      EEPROM.write(a_dPO, 1);   disattivaPrimaOra = true;
    }
    else {
      EEPROM.write(a_dPO, 0); disattivaPrimaOra = false;
    }
  }
  if (setOrologio) setRTC(secondi, minuti, ora, gSett, giorno_mese, mese, anno);
  if (setOrarioLezioni)
  { for (int i = 0; i < 2; i++)
      for (int j = 0; j < 2; j++) EEPROM.write(a_orarioLezioni + (i * 2 + j), orarioLezioni[i][j]);
  }
  for (int i = 0; i < 2; i++)
    orarioLezioni[i][2] = orarioLezioni[i][0] * 60 + orarioLezioni[i][1];
  if (setRestart) saveEeprom2SD();
  if (setOrologio || setOrarioLezioni || setRestart)
  { wdt_reset();
    setWDT(_2secRes); 
    delay(3000);
  }
}

/*___________________________________________________________________________
visualizzaChiudiSetup
visualizza form per logout da setup
*/
void visualizzaChiudiSetup(EthernetClient client)
{
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println();
  client.println(F("<HTML>"));
  client.println(F("<BODY>"));
  client.println(F("Impostazioni salvate"));
  client.println(F("<br><br>"));
  client.println(F("<form name=\"prog\" METHOD= \"GET\">"));
  client.println(F("<br>"));
  if (selettore == "prg")
      client.println(F("<input type=\"submit\" value=\"programmazione classi\" >"));
  if (selettore == "set") 
      client.println(F("<input type=\"submit\" value=\"setup\" >"));
  client.println(F("</form>"));
  client.println(F("<br><br>"));
  client.println(F("<a href=\"?lgt\">logout</a>"));
  client.println(F("</BODY>"));
  client.println(F("</HTML>"));
}
 
/*___________________________________________________________________________
visualizzaPaginaProgr
visualizza form per programmazione ingresso classi in ritardo;
restituisce posizione prossimo record da inserire
*/ 
int visualizzaPaginaProgr(EthernetClient client)
{ int nextIndice;
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println();
  client.println(F("<HTML>"));
  client.println(F("<BODY>"));
  client.println(F("<H1> I S I S   \" D I   M A G G I O \"</H1>"));
  client.println(F("<br>"));
  client.println(F("<H2> LETTORE  RFID CARD </H2>"));
  client.println(F("<br>"));
  client.println(F("<H3> PROGRAMMAZIONE ORARIO INGRESSO CLASSI </H3>"));
  client.println(F("<H5>"));
  client.println(F("<br>"));
  client.println(F("    classe ora min giorno mese anno <br>"));
  client.println(F(" ____________________________________________<br>"));
  for (int i = 0; i < 16; i++)
  {
    if ((classe[i] == "fine") || (classe[i].length() < 1))
    { nextIndice = i;
      break;
    }
    client.print(i + 1); client.print(":  ");
    client.print(classe[i]);
    client.print("  ");
    for (int j = 0; j < 5; j++)
    {
      client.print(orarioIngrClassi[i][j]);
      client.print("  ");
    }
    client.println("<br>");
  }
  client.println(F("<form name=\"admin\" METHOD= \"GET\">"));
  client.println(F("<br>"));
  client.println(F("<TABLE>"));
  client.println(F("<TR><TD>Classe</TD><TD><input type=\"text\"  name=\"cl1\" maxlength=\"3\" size=\"3\"</TD>"));
  client.println(F("<TD>ora ingresso</TD><TD><input type=\"text\"  name=\"ocl1\" maxlength=\"2\" size=\"2\"</TD>"));
  client.println(F(":<TD><input type=\"text\"  name=\"mcl1\" maxlength=\"2\" size=\"2\"</TD>"));
  client.println(F("<TD>giorno</TD><TD><input type=\"text\"  name=\"gcl1\" maxlength=\"2\" size=\"2\">"));
  client.print(F("/<input type=\"text\"  name=\"mcl1\" maxlength=\"2\" size=\"2\">"));
  client.print(F("/<input type=\"text\" maxlength=\"2\" size=\"2\" name=\"acl1\"></TD></TR></TABLE>"));

  client.println(F("<br>"));
  client.print(F("cancella elemento n.<input type=\"text\"  name=\"ind\" maxlength=\"2\" size=\"2\" value=\"*\">"));
  client.println(F("<br>"));
  client.println(F("<input type=\"submit\" value=\"salva\" >"));
  client.println(F("</form>"));
  client.println(F("</BODY>"));
  client.println(F("</HTML>"));
  return nextIndice;
}

/*___________________________________________________________________________
leggiProgrClassi
routine per acquisire le classi che entreranno in ritardo:
-se cod =0 acquisisce da form;
-se cod <> 0 acquisisce da registro elettronico.
-il parametro i rappresenta il nuovoIndice, cioè la posizione in cui scrivere la nuova classe e orario
  la funzione restituisce 0 : il file è terminato correttamente con 'fine'
                          1 tutto ok per quella iterazione e può continuare
                          2 errore nella ricezione del file
*/
byte leggiProgrClassi(EthernetClient client, int i, byte cod)
{byte cont =0;
  if (client.find("cl1="))  {classe[i] = client.readStringUntil('&');  cont++;}
  if (classe[i].equals("fine")) return 0;     //termina perchè ha ricevuto 'fine'
  if (client.find("ocl1=")) { orarioIngrClassi[i][0] = client.readStringUntil('&').toInt(); cont++;} //ora
  if (client.find("mcl1=")) { orarioIngrClassi[i][1] = client.readStringUntil('&').toInt(); cont++;} //minuti
  if (client.find("gcl1=")) { orarioIngrClassi[i][2] = client.readStringUntil('&').toInt(); cont++;} //giorno
  if (client.find("mcl1=")) { orarioIngrClassi[i][3] = client.readStringUntil('&').toInt(); cont++;} //mese
  if (client.find("acl1=")) { orarioIngrClassi[i][4] = client.readStringUntil('&').toInt(); cont++;} //anno
  if (cont != 6) return 2;  
  orarioIngrClassi[i][5] = orarioIngrClassi[i][0] * 60 + orarioIngrClassi[i][1];
  if (cod == 0)              
   { 
    classe[i + 1] = "fine";
    if (client.find("ind=")) 
    { String s = client.readStringUntil(' ');
      if (s != "*")
      eliminaElementoVettore( s.toInt() - 1);
    }
  salvaClassiSD();
   }
  return 1;  
}

/*___________________________________________________________________________
salvaClassiSD
salva su SD le classi che entreranno in ritardo acquisite con le funzioni precedenti
*/ 
void salvaClassiSD()
{
  digitalWrite(chipSelectEth, HIGH); 
  digitalWrite(chipSelectSD, LOW); 
  if (SD.exists("orarioIC.txt")) SD.remove("orarioIC.txt");
  dataFileOr = SD.open("orarioIC.txt", FILE_WRITE);
  if (dataFileOr)
  { 
    for (int i = 0; i < nuovoIndice + 2; i++) 
    { 
      dataFileOr.println(classe[i]);
      for (int j = 0; j < 6; j++)
      {
        dataFileOr.println(orarioIngrClassi[i][j]);
      }
    }
    dataFileOr.flush();
    dataFileOr.close();
  }
  else
  { lcd.setCursor(0, 0);
    lcd.print("errore file SD");
  }
  dataFileOr.close();
  digitalWrite(4, HIGH); 
  digitalWrite(chipSelectEth, LOW); 
}

/*___________________________________________________________________________
caricaOrarioIngrClassi
all'accensione carica classi che entreranno in ritardo da SD
*/ 
void caricaOrarioIngrClassi()
{ //     Serial.println("carica orario ingresso classi");
  digitalWrite(chipSelectEth, HIGH); //disattiva ethernet
  digitalWrite(chipSelectSD, LOW); //attiva SD
  dataFileOr = SD.open("orarioIC.txt", FILE_WRITE);
  if (dataFileOr && dataFileOr.size() > 3)
  { 
    dataFileOr.seek(0);
    int i = 0; int k = 0;
    do
    { i = k;
      classe[i] = dataFileOr.readStringUntil(0xD);     
      if (classe[i] == "fine" || classe[i].length() == 0) break;
      for (int j = 0; j < 6; j++)
      {
        orarioIngrClassi[i][j] = dataFileOr.readStringUntil(0xD).toInt();
        dataFileOr.read();
      }
      if (mese < orarioIngrClassi[i][3] || (orarioIngrClassi[i][3] == mese && orarioIngrClassi[i][2] >= giorno_mese))
        k++;
    }
    while ((classe[i] != "fine") && (classe[i].length() != 0)) ;
    dataFileOr.close();
  }
  else
  { // Serial.println("file orario ingresso non presente");
  }
  dataFileOr.close();
  digitalWrite(4, HIGH); 
  digitalWrite(chipSelectEth, LOW); 
}

/*___________________________________________________________________________
caricaSetup
all'accensione carica i valori di setup memorizzati in eeprom
*/ 
void caricaSetup()
{
  istituto = "";
  int j = 0;
  while (EEPROM.read(a_istituto + j) != 255)
  {
    istituto = istituto + (char) EEPROM.read(a_istituto + j);
    j++;
  }
  for (int i = 0; i < 2; i++)
     for (int j = 0; j < 2; j++) orarioLezioni[i][j] = EEPROM.read(a_orarioLezioni + (i * 2 + j));
  for (int i = 0; i < 2; i++)
     orarioLezioni[i][2] = orarioLezioni[i][0] * 60 + orarioLezioni[i][1];
  mRitIngresso = EEPROM.read(a_mRitIngresso );
  mRitTx = EEPROM.read(a_mRitTx );
  j = 0;
  ipWebServer = "";
  while (EEPROM.read(a_ipWebServer + j) != 255)
  {
    ipWebServer = ipWebServer + (char) EEPROM.read(a_ipWebServer + j);
    j++;
  }
  j = 0;
  dirWebServer = "";
  while (EEPROM.read(a_dirWebServer + j) != 255)
  {
    dirWebServer = dirWebServer + (char) EEPROM.read(a_dirWebServer + j);
    j++;
  }
  j = 0;
  dir2WebServer = "";
  while (EEPROM.read(a_dir2WebServer + j) != 255)
  {
    dir2WebServer = dir2WebServer + (char) EEPROM.read(a_dir2WebServer + j);
    j++;
  }
  j = 0; 
  dir3WebServer = "";
  while (EEPROM.read(a_dir3WebServer + j) != 255)
  {
    dir3WebServer = dir3WebServer + (char) EEPROM.read(a_dir3WebServer + j);
    j++;
  }  
  if ( EEPROM.read(a_dPO) == 0) disattivaPrimaOra = false;
  if ( EEPROM.read(a_dPO) == 1) disattivaPrimaOra = true;
  if ( EEPROM.read(a_aggClass) == 0) aggiornaClasseAttivo = false;
  if ( EEPROM.read(a_aggClass) == 1) aggiornaClasseAttivo = true;
  disattMin = EEPROM.read(a_disattMin);
  durata = EEPROM.read(a_durata);
  maskRegAbil = EEPROM.read(a_maskRegAbil);
  if (EEPROM.read(a_selFile) == 255) selFile = "br";
  if (EEPROM.read(a_selFile) == 0) selFile = "ws";
}
/*___________________________________________________________________________
inviaDatiWebFile
invia dati al registro elettronico leggendoli dal file su SD
formato record a campi variabili per ogni registrazione:  MatricolaCodiceOra;
-matricola 
-codice: I ingresso regolare; U uscita; R ingresso in ritardo
-ora: ora, minuti, secondi.
*/ 
boolean inviaDatiWebFile()
{ EthernetClient clientws;
  int i; byte cont = 0;
  int k = 0; //contatore per uscire dal ciclo se registro elettronico non risponde
  fileTrasmesso = false;
  spegniLed();
  visualizzaWait("..In trasmissione...", monitorAttivo);
  wdt_reset();

  String m1 = "";
  boolean confermaDatiRicevuti, datiWebTrasmessi;
  digitalWrite(chipSelectEth, HIGH); 
  digitalWrite(4, LOW); 

  dataFileM = SD.open("presmem", FILE_WRITE);
  dataFilePr = SD.open("presenze", FILE_WRITE);
  dataFileM.seek(0);
  puntFilePres =  dataFileM.readStringUntil(';').toInt();   
  int ultimaPosFile = dataFilePr.size();
  int posInizioTx = puntFilePres;
  int posPrecedente;
  while (fileOK && !fileTrasmesso)
  { wdt_reset();
    digitalWrite(chipSelectEth, HIGH); 
    digitalWrite(chipSelectSD, LOW); 
    datiWebTrasmessi = false;
    posPrecedente = posInizioTx;                                          
    dataFilePr.seek(posInizioTx);
    i = 0;
    do
    {
      m1 = m1 + dataFilePr.readStringUntil(0xD);                          
      dataFilePr.read();
      i++;
    }
      while ((dataFilePr.position() < ultimaPosFile) && i < 30);   //legge 30 record alla volta
    if (dataFilePr.position() >= ultimaPosFile)
    {                                                               
      fileTrasmesso = true;
      monitorTX(16, 'T');
    }
    posInizioTx = dataFilePr.position();              
    if (m1.compareTo("") == 0)   break;

    md5String = calcolaMD5(m1);
    String invia = "key=" + md5String + "&m1=" + m1;

    digitalWrite(4, HIGH); 
    digitalWrite(chipSelectEth, LOW); 
  
    wdt_reset();
    if (clientws.connect(ipWebServerC, 80))
    {
      monitorTX(8, 'C');
      String host = "Host: " + ipWebServer;
      String dir2EFile = "POST /" + dir2WebServer + " HTTP/1.1";  
      clientws.println(dir2EFile);
      clientws.println(host);             
      clientws.println("User-Agent: Arduino/1.0");            
      clientws.println("Connection: close");                   
      clientws.println("Content-Type: application/x-www-form-urlencoded"); 
      clientws.print("Content-Length: ");                      
      clientws.println(invia.length());
      clientws.println();
      clientws.print(invia);
      clientws.println();

      datiWebTrasmessi = true;                                                    
    }
    else 
    { 
      monitorTX(9, 'F');
      Serial.println("connection failed3");
     }
 
    richiestiDatiWeb = false;

    int j = 0;
    while (!clientws && j < 100)
    { delay(5);
      j++;
      wdt_reset();                              
    }
    if (clientws && datiWebTrasmessi)
    { 
      monitorTX(11, 'A');
      while (clientws.connected())
      { wdt_reset();
        if (clientws.available())
        {
          if (clientws.find("ricevuti"))
          { cont++;
            //monitor tx
            monitorTX(12, 'R');
            monitorTX(13, (byte) cont);
            datiWebTrasmessi = false;
            confermaDatiRicevuti = true;
            newRecSD = false;                                                      
            clientws.stop();
            k = 0;

            digitalWrite(chipSelectEth, HIGH); 
            digitalWrite(4, LOW);          
            dataFileM.seek(0);
            dataFileM.print(posInizioTx);
            dataFileM.print(';');
            digitalWrite(4, HIGH); 
          }
        }
        else confermaDatiRicevuti = false;
        if (!clientws.connected())
        {
          monitorTX(15, 'D');
          clientws.stop();
          break;
        }

      } 
    }
    else
    { 
      confermaDatiRicevuti = false;
    }
    if (!confermaDatiRicevuti)
    {
      posInizioTx = posPrecedente; 
      fileTrasmesso = false;
      
      k++;            
      monitorTX(10, (byte) k);
    }
    if (k >= 3)
    { 
      break;
    }
  
    m1 = "";
  if (unaTx)  { break;}
  }
  digitalWrite(chipSelectEth, HIGH);
  digitalWrite(4, LOW); 
  dataFilePr.close();
  dataFileM.close();
  accendiLed();
  visualizzaDisplay();
  digitalWrite(4, HIGH); 
  digitalWrite(chipSelectEth, LOW); 

  if (fileTrasmesso) return true;
  else
  {                                                              
    return false;
  }
}
/*___________________________________________________________________________
richiediDatiWeb
Richiesta a registro elettronico (RE) per ottenere: 
-autorizzazione a TX le timbrature. In questo caso si aspetta di ricevere dal RE la parola 'trasmetti'
-file delle classi che entreranno in ritardo. in questo caso riceverà le informazioni con il seguente formato:
    classe;ora;minuti;giorno;mese;anno;orainminuti;classe;ora;......... e così via. Per finire invece della classe riceverà 'fine'.
Il parametro cod = 0 -> richiesta autorizzazione a trasmettere
             cod = 1 -> richiesta file classi
*///********************************************************************************************************
void richiediDatiWeb(byte cod)
{ EthernetClient clientws;                                
  String invia2;
  wdt_reset();
  setWDT(_8secIntRes);

  lcd.setCursor(0, 2); lcd.print("prova coll. internet");
  digitalWrite(chipSelectSD, HIGH); 
  digitalWrite(chipSelectRfid, HIGH); 
  digitalWrite(chipSelectEth, LOW); 

  lcd.setCursor(0, 2); lcd.print("                    ");
  monitorTX(0, 'R');
  monitorAttivo = true;
  timer2minMem = timer2min;
  if (cod ==0)  invia2 = "GET /" + dirWebServer + " HTTP/1.1";   
  else if (cod ==1) invia2 = "GET /" + dir3WebServer + " HTTP/1.1";
  String host = "Host: " + ipWebServer;
  digitalWrite(4, HIGH); 
  digitalWrite(chipSelectEth, LOW); 
  for (char i = '0'; i < '3'; i++)
  {
    monitorTX(1, i);                     
    if (clientws.connect(ipWebServerC, 80))
    { 
      monitorTX(4, 'C');
      clientws.println(invia2);
      clientws.println(host);
      clientws.println("Connection: close");
      clientws.println();
      richiestiDatiWeb = true;           
      break;   
    }
  }
  if (!richiestiDatiWeb)
  {
    monitorTX(5, 'F');
    lcd.setCursor(0, 1); lcd.print("cavo eth. scollegato");
    trasmettiWS = false;
    return;
  }
 
  wdt_reset();
  while (clientws.connected())
  {if (cod ==0)      
   {if (clientws.find("trasmetti"))
    { clientws.stop();
      monitorTX(6, 'T');
     
      if (inviaDatiWebFile())
      { trasmettiWS = false;
        contatoreNTx = 0;
      }
      else  if (unaTx) {trasmettiWS = false; unaTx = false;}  
            else { trasmettiWS = true;}   
    } 
    else 
        {trasmettiWS = true; 
         lcd.setCursor(0, 1); lcd.print("no ricev. -trasmetti");     
        }
 }
  else if (cod == 1)  
        if (riceviFileClassi(clientws))   erroreRicezioneFileClassi = false; else erroreRicezioneFileClassi = true;
 }
 clientws.stop();
}

/*___________________________________________________________________________
timeDateToString
calcola l'ora da memorizzare nel file presenze insieme alla matricola e al codice
-modificata per eliminare dal file presenze e quindi dalla trasmissione al registro elettronico il giorno_mese e il mese
-calcola anche da data nel formato ggmmaa da accodare all'MD5
*/
void timeDateToString()
{
  String temp = (String) ora;
  if (temp.length() == 1) {
    timeDateStringa[0] = '0';
    timeDateStringa[1] = temp[0];
  }
  else {
    timeDateStringa[0] = temp[0];
    timeDateStringa[1] = temp[1];
  }

  temp = (String) minuti;
  if (temp.length() == 1) {
    timeDateStringa[2] = '0';
    timeDateStringa[3] = temp[0];
  }
  else {
    timeDateStringa[2] = temp[0];
    timeDateStringa[3] = temp[1];
  }

  temp = (String) giorno_mese;
  if (temp.length() == 1) {
    timeDateStringa[4] = '0';
    timeDateStringa[5] = temp[0];
  }
  else {
    timeDateStringa[4] = temp[0];
    timeDateStringa[5] = temp[1];
  }

  temp = (String) mese;
  if (temp.length() == 1) {
    timeDateStringa[6] = '0';
    timeDateStringa[7] = temp[0];
  }
  else {
    timeDateStringa[6] = temp[0];
    timeDateStringa[7] = temp[1];
  }
  temp = (String) anno;
  if (temp.length() == 1) {
    timeDateStringa[8] = '0';
    timeDateStringa[9] = temp[0];
  }
  else {
    timeDateStringa[8] = temp[0];
    timeDateStringa[9] = temp[1];
  }  
  
  for (int i =0; i < 4; i++) timeStringa[i] = timeDateStringa[i]; 
  for (int i =0; i < 6; i++) timeStringaXMD5[i] = timeDateStringa[i+4]; 
}

/*___________________________________________________________________________
calcolaProgrGiorni
routine per calcolare un progressivo di sistema utilizzato per:
  -evitare che passando più volte la tessera lo stesso giorno venga incrementato progrPresenza;
  -riconoscere se si passa la card per uscita anticipata.
  viene richiamato ad ogni setup e ogni volta che passa un giorno
  Nella routine originale il valore viene incrementato ogni volta che passa un giorno scolastico.
  Con 2 totem il valore deve essre uguale per entrambi per evitare che strisciando sull'altro totem
  successivamente venga considerato ancora in ingresso.
  Pertanto il valore deve essere una funzione del giorno.
  La funzione è: (m * 31 + g ) mod 255 con m mese e g giorno; se m > 8 m= m - 9 ; se m < 7 m= m+3
  In questo modo si ha un numero crescente con il passare dei giorni scolastici. A maggio si ritorna a
  contare da 1 quando si supera 255.
  Non è necessario memorizzare su eeprom in quanto ogni volta che si accende il dispositivo viene effettuato il calcolo.
*/
void calcolaProgrGiorni()
{
  if ( mese > 8)
  {
    progrGiorni = ((mese - 9) * 31 + giorno_mese) % 255;
  }
  else
  {
    progrGiorni = ((mese + 3) * 31 + giorno_mese) % 255;
  }
}

/*___________________________________________________________________________
eliminaElementoVettore
elimina un elemento dei vettori paralleli per orario ingresso classi perchè data trascorsa 
*/ 
void eliminaElementoVettore(int i)
{
  do
  { classe [i] = classe[i + 1];
    for (int j = 0; j < 6; j++)
      orarioIngrClassi[i][j] = orarioIngrClassi[i + 1][j];
    i++;
  }

  while (classe[i - 1] != "fine");
}

/*___________________________________________________________________________
verificaSeRitardo
verifica in base all'orario, ai dati programmati sulla card e all'orario ingresso classi, se la 
timbratura di quella card risulta in orario o in ritardo
*/ 
boolean verificaSeRitardo()
{
   if ((blockData[iFuorisede] == 0 && oraInMinuti < maxOrarioIngresso) || 
       (blockData[iFuorisede] != 0 && oraInMinuti < (maxOrarioIngresso -mRitIngresso + blockData[iFuorisede])))
   return false;        //nei limiti non ritardo
   
  else
  { 
    String o = "00";
    String m = "00";
    byte i = 48 + (giorno_sett - 1) * 4; 
    o.setCharAt(0, blockData[i]);
    o.setCharAt(1, blockData[i + 1]);          
    m.setCharAt(0, blockData[i + 2]);
    m.setCharAt(1, blockData[i + 3]);         

    if (oraInMinuti < (o.toInt() * 60 + m.toInt()))   
    {
      return false; 
    }
    else
    { 
      for (int i = 0; i < 16; i++)
      {
        bool classeUguale = false;
        for (int k = 0; k < classe[i].length(); k++)
        { if (classeS[k] == classe[i][k]) classeUguale = true;
          else  {
            classeUguale = false;
            break;
          }
        }
        if (classeUguale && (orarioIngrClassi[i][2] == giorno_mese) && (orarioIngrClassi[i][3] == mese) &&
            (orarioIngrClassi[i][4] == anno))
        { 
          if (oraInMinuti < orarioIngrClassi[i][5] + mRitIngresso) {
            return false;
          }
          break;
        }
      }

    }
    return true;
  }
}

/*___________________________________________________________________________
setRTC
imposta l'orologio di sistema (RTC) in base al setup (da browser) o interrogazione a web server (vedi aggiornaOra()).
//input: dati sotto forma di caratteri
*/ 
void setRTC(byte s, byte mi, byte h, String gs, byte g, byte me, byte a)
{ byte gsB;
  if (gs == "DOM") gsB = 7;
  if (gs == "LUN") gsB = 1;
  if (gs == "MAR") gsB = 2;
  if (gs == "MER") gsB = 3;
  if (gs == "GIO") gsB = 4;
  if (gs == "VEN") gsB = 5;
  if (gs == "SAB") gsB = 6;
  //conversione da byte a BCD (2 cifre in un byte)
  byte sec = (s / 10) << 4 | (s % 10);
  byte m = (mi / 10) << 4 | (mi % 10);
  byte ora = (h / 10) << 4 | (h % 10);
  byte gse = (gsB / 10) << 4 | (gsB % 10);
  byte gi = (g / 10) << 4 | (g % 10);
  byte mes = (me / 10) << 4 | (me % 10);
  byte an = (a / 10) << 4 | (a % 10);

  Wire.beginTransmission(0x68);
  Wire.write((byte)0x00);
  Wire.write(sec); 
  Wire.write(m); 
  Wire.write(byte(0x00 | ora)); 
  Wire.write(gse); 
  Wire.write(gi); 
  Wire.write(mes); 
  Wire.write(an); 
  Wire.endTransmission();
  leggiRTC();
}

/*___________________________________________________________________________
aggiornaOra
collegamento a server google.it per prelevare e aggiornare ora e data 
*/ 
void aggiornaOra()
{ byte h, mi, sec, gi, me, a;
  char c, AMPM;
  bool oraLetta = false;
  String  gse, temp;
  for (int i = 0; i < 3; i++)
  { 
    if (clienth.connect(serverCK, 80)) 
    { Serial.println("connected");
      clienth.println("GET /it_fuso_orario/italy_sassari_ora.php HTTP/1.0");//pagina inesistente su google ma ci interessa l'ora
      clienth.println();
      break;
    }
    else 
    {
 //     Serial.println("connection failed2");
    }
  }
  for (int i = 0; i < 3; i++)
  { 
    if (clienth)
    {
      while (clienth.connected())
      { if (clienth.available())
        { if (clienth.find("Date: "))
          { gse = clienth.readStringUntil(',');
            clienth.read(); 
            gi = clienth.readStringUntil(' ').toInt();
            temp = clienth.readStringUntil(' '); 
            a = clienth.readStringUntil(' ').substring(2).toInt();
            h = clienth.readStringUntil(':').toInt();
            mi = clienth.readStringUntil(':').toInt();
            sec = clienth.readStringUntil(' ').toInt();
            oraLetta = true;
            if (gse == "Sun" ) gse = "DOM";
            else if (gse == "Mon" ) gse = "LUN";
            else if (gse == "Tue" ) gse = "MAR";
            else if (gse == "Wed" ) gse = "MER";
            else if (gse == "Thu" ) gse = "GIO";
            else if (gse == "Fri" ) gse = "VEN";
            else if (gse == "Sat" ) gse = "SAB";
            if (temp == "Jan") me = 1;
            else if (temp == "Feb") me = 2;
            else if (temp == "Mar") me = 3;
            else if (temp == "Apr") me = 4;
            else if (temp == "May") me = 5;
            else if (temp == "Jun") me = 6;
            else if (temp == "Jul") me = 7;
            else if (temp == "Aug") me = 8;
            else if (temp == "Sep") me = 9;
            else if (temp == "Oct") me = 10;
            else if (temp == "Nov") me = 11;
            else if (temp == "Dec") me = 12;
            if (h < 23) h++; else h = 0;
            break;  
          } 
        } 
      }
      if (oraLetta)
      { clienth.stop();
        setRTC(sec, mi, h, gse, gi, me, a);
        break; 
      }
    }
    delay(100);  
  }
  if (oraLetta)
  {
    lcd.setCursor(0, 3);
    lcd.print("aggiornamento ora OK");
    if (verificaOraLegale(mese, giorno_mese, gSett)) //ora legale e quindi ora di internet va aumentata di 2 ore
    { 
      if (EEPROM.read(a_memLegale) == 0) EEPROM.write(a_memLegale, 1);
      ora++;
    }
    else if (EEPROM.read(a_memLegale) == 1) EEPROM.write(a_memLegale, 0);//passaggio da ora legale a ora solare
    setRTC(secondi, minuti, ora, gSett, giorno_mese, mese, anno);
    return;
  }
  else
  {
    lcd.setCursor(0, 3);
    lcd.print(" aggior.ora failed ");
    if (verificaOraLegale(mese, giorno_mese, gSett))
    { 
      if (EEPROM.read(a_memLegale) == 0)
      { EEPROM.write(a_memLegale, 1);
        ora++;
        setRTC(secondi, minuti, ora, gSett, giorno_mese, mese, anno);
      }
    }
    else  
    { 
      if (EEPROM.read(a_memLegale) == 1) 
      { ora--;
        EEPROM.write(a_memLegale, 0);
        setRTC(secondi, minuti, ora, gSett, giorno_mese, mese, anno);
      }
    }
    return ;
  }
}

/*___________________________________________________________________________
leggiRTC
legge orologio di sistema
*/ 
void leggiRTC()
{
  Wire.begin();
  Wire.beginTransmission(0x68);
  Wire.write(byte(0x07)); 
  Wire.write(B00010000); 
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write((byte)0x00);
  Wire.endTransmission();
  Wire.requestFrom(0x68, 7);

  secondi = Wire.read();
  minuti = Wire.read();
  ora = Wire.read();
  giorno_sett = Wire.read();
  giorno_mese = Wire.read();
  mese = Wire.read();
  anno = Wire.read();
 
  byte temp = secondi & B00001111;
  secondi = secondi >> 4;
  secondi = secondi * 10 + temp;

  temp = minuti & B00001111;
  minuti = minuti >> 4;
  minuti = minuti * 10 + temp;

  temp = ora & B00001111;
  ora = ora >> 4;
  ora = ora * 10 + temp;

  temp = giorno_mese & B00001111;
  giorno_mese = giorno_mese >> 4;
  giorno_mese = giorno_mese * 10 + temp;

  temp = mese & B00001111;
  mese = mese >> 4;
  mese = mese * 10 + temp;

  temp = anno & B00001111;
  anno = anno >> 4;
  anno = anno * 10 + temp;
 
  switch (giorno_sett)
  {
    case 1:
      { gSett = "LUN";
        break;
      }
    case 2:
      { gSett = "MAR";
        break;
      }
    case 3:
      { gSett = "MER";
        break;
      }
    case 4:
      { gSett = "GIO";
        break;
      }
    case 5:
      { gSett = "VEN";
        break;
      }
    case 6:
      { gSett = "SAB";
        break;
      }
    case 7:
      { gSett = "DOM";
        break;
      }
  }
  attachInterrupt(0, clock, RISING); 
}

/*___________________________________________________________________________
calcolaDataXDisplay
calcola una stringa contenente la data per il display
*/ 
String calcolaDataXDisplay()
{
  String s = "                    ";
  s = (String) giorno_mese;
  switch (mese)
  {
    case 1: s.concat("  GENNAIO  "); break;
    case 2: s.concat("  FEBBRAIO "); break;
    case 3: s.concat("  MARZO    "); break;
    case 4: s.concat("  APRILE   "); break;
    case 5: s.concat("  MAGGIO   "); break;
    case 6: s.concat("  GIUGNO   "); break;
    case 7: s.concat("  LUGLIO   "); break;
    case 8: s.concat("  AGOSTO   "); break;
    case 9: s.concat("  SETTEMBRE "); break;
    case 10: s.concat("  OTTOBRE "); break;
    case 11: s.concat("  NOVEMBRE "); break;
    case 12: s.concat("  DICEMBRE "); break;
  }
  s.concat("20");
  s.concat((String) anno);
  return s;
}

/*___________________________________________________________________________
visualizzaDisplay
visualizza stringhe di default su display
*/
void visualizzaDisplay()
{
  lcd.setCursor(0, 0); lcd.print("I.I.S.S. 'DI MAGGIO'" );
  lcd.setCursor(0, 1); lcd.print("Rilevazione presenze" );
  lcd.setCursor(0, 2); lcd.print("                    ");
  lcd.setCursor(0, 2); lcd.print(dataDisplay);
}

/*___________________________________________________________________________
visualizzaWait
visualizza stringa 'WAIT PLEASE'
*/ 
void visualizzaWait(String s, boolean monitortx)
{
  lcd.setCursor(0, 0); lcd.print("WAIT PLEASE         ");
  lcd.setCursor(0, 1); lcd.print(s);
  if (!monitortx)
  { lcd.setCursor(0, 2); lcd.print("                    ");
    lcd.setCursor(0, 2); lcd.print(dataDisplay);
  }
}

/*___________________________________________________________________________
visualizzaDis
visualizza stringa 'INGRESSO DISATTIVATO'
*/ 
void visualizzaDis(byte ora, byte minuti)
{
  lcd.setCursor(0, 0); lcd.print("INGRESSO DISATTIVATO" );
  lcd.setCursor(0, 1); lcd.print("                    ");
  lcd.setCursor(0, 1); lcd.print("  attivo alle ");
  lcd.print(ora); lcd.print("."); lcd.print(minuti);
  lcd.setCursor(0, 2); lcd.print("                    ");
  lcd.setCursor(0, 2); lcd.print(dataDisplay);
}

/*___________________________________________________________________________
accendiLed
accende i led ad alta luminosità installati nel case
*/ 
void accendiLed()
{
  digitalWrite(led1, LOW);
  digitalWrite(led3, LOW);
  digitalWrite(led4, LOW);
}
/*___________________________________________________________________________
spegniLed
spegne i led ad alta luminosità installati nel case
*/ 
void spegniLed()
{
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);
  digitalWrite(led4, HIGH);
}

/*___________________________________________________________________________
startMelody
melodia in fase di accensione
*/ 
void startMelody()
{
  int melody[] = {NOTE_E5, NOTE_E4, NOTE_B4, NOTE_E4, NOTE_A4, NOTE_E4, NOTE_E5, NOTE_B4};
  int noteDurations[] = {
    2, 4, 2, 4, 2, 2, 2, 1
  };

  for (int thisNote = 0; thisNote < 8; thisNote++) {

    int noteDuration = 1000 / noteDurations[thisNote];
    tone(buz, melody[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 0.50;
    delay(pauseBetweenNotes);

    noTone(buz);
  }
}

/*___________________________________________________________________________
verificaOraLegale
routine per determinare se ora legale per correzione ora
*/ 
boolean verificaOraLegale(byte mese, byte giorno, String giornoSett)
{ byte domSucc;
  if (mese == 3 || mese == 10)
  { if (giornoSett == "DOM") domSucc = giorno + 7;
    if (giornoSett == "LUN") domSucc = giorno + 6;
    if (giornoSett == "MAR") domSucc = giorno + 5;
    if (giornoSett == "MER") domSucc = giorno + 4;
    if (giornoSett == "GIO") domSucc = giorno + 3;
    if (giornoSett == "VEN") domSucc = giorno + 2;
    if (giornoSett == "SAB") domSucc = giorno + 1;
  }
  if ((mese > 3 && mese < 10) || (mese == 3 && domSucc > 31) || (mese == 10 && domSucc < 32))
  { 
    return true;
  }
  else
  { 
    return false;
  }
}

/*___________________________________________________________________________
monitorTX
monitor a caratteri su display per monitoraggio trasmissione a registro elettronico 
*/ 
void monitorTX(byte pos, char val)
{
  lcd.setCursor(pos, 2); lcd.print(val);
}

/*___________________________________________________________________________
aggiornaClasse
se abilitata in setup, scrive nella card la nuova classe ( ad inizio anno scolastico) prelevandola dal file aggclass.txt in SD
*/ 
bool aggiornaClasse()
{ int matrInt;
  String classeSFile;
  for (int j = 38; j < 43; j++) 
  {
    matrS.setCharAt(j - 38, *(punt + j));
  }
  matrInt = matrS.toInt();  

  digitalWrite(chipSelectEth, HIGH); 
  digitalWrite(chipSelectSD, LOW); 
  dataFileClasse = SD.open("aggclass.txt", FILE_READ);
  if (dataFileClasse)
  { if (dataFileClasse.size() > 0)
    { 
      int puntatore = (matrInt - primaMatrFile) * 4 + 6;  
      if (puntatore < dataFileClasse.size())  
      { 
        dataFileClasse.seek(puntatore);
        classeSFile = dataFileClasse.readStringUntil(';'); 
        if (!classeS.equals(classeSFile)&& !classeS.equals("ERR"))
        { 
          for (int j = 43; j < 46; j++) 
          {
            blockData[j] = classeSFile.charAt(j - 43);
          }
          blockData[iProgrPresenze] = 0;  
          blockData[iProgrRitardi] = 0;
          dataFileClasse.close();
          return true;
        }
      }
    }
  }
  else { //Serial.println("file aggclassi non aperto");
       }
  dataFileClasse.close();
  return false;
}

/*___________________________________________________________________________
verificaFileAggClass
routine per la verifica della correttezza formale del file aggclass.txt: primo record di 5 caratteri, separatore ';' e successivamente
tutti gli altri record costituiti da 3 caratteri con separatore ';'. Inoltre setta la variabile primaMatrFile.
*/ 
int verificaFileAggClass()
{ String tempS; int i = 1; int j = 0;
  lcd.setCursor(0, 1);      lcd.print("verif .file aggclass ");
  delay(1000);
  dataFileClasse = SD.open("aggclass.txt", FILE_READ);
  if (dataFileClasse.size() > 0)
  { 
    tempS =  dataFileClasse.readStringUntil(';');
    if (tempS.length() != 5) {
      return (i);     
    }
    primaMatrFile = tempS.toInt(); 
  }
  if (coldStart)
  {
    while (dataFileClasse.position() < dataFileClasse.size() - 1)
    { i++;                                                             
      if (dataFileClasse.readStringUntil(';').length() != 3)
      {
        Serial.println("file aggclass; errore matr.");
        Serial.println(i);
        j = i;
        break;
      }
    }
  }
  dataFileClasse.close();
  return (j);
}

/*___________________________________________________________________________
setMaskOrarioAbil
in base all'ora ricava una maschera binaria da confrontare con un codice registro (programmato in card):
il corso serale con CR 1 non deve timbrare di mattina e il mattino con codice 16 la sera.
*/ 

byte setMaskOrarioAbil(int h)
{ byte m =0;
  if (h >= 7 && h < 15) m = B11110000;  //al mattino possono essere abilitati 4 registri diversi che corrispondono a 16,32,64,128
  if (h >= 15 && h < 21) m = B00001111;//al serale possono essere abilitati 4 registri diversi che corrispondono a 1,2,4,8
  callSetMaskOrarioAbil = false;
  return m;
}

/*___________________________________________________________________________
riceviFileClassi
richiama la funzione per ricevere il file entrata classi in ritardo dal registro elettronico
*/ 
bool riceviFileClassi(EthernetClient client)
{ int i = 0; byte risposta;
  while (client.connected())
  {
   risposta = leggiProgrClassi(client, i, 1);    
   if (risposta == 1) i++;
   else if (risposta == 0)
          {
           salvaClassiSD();
           return true;  
          }
          else return false; 
  }
  
}

/*___________________________________________________________________________
calcolaMD5
calcola MD5 della stringa contenente le timbrature da inviare al registro elettronico
*/ 
String calcolaMD5( String s)
{ int lun = s.length(); 
  char arrayEData[370];
  arrayEData[369] = '\0';
  for (int i=0; i< 369; i++) arrayEData[i] = '$';
  for (int i = 0; i < 32; i++) arrayEData[i] = EEPROM.read(a_md5 +i);
  for (int i=0; i< lun; i++) arrayEData[i+32] = s.charAt(i);
  for (int i = 0; i < 6; i++) arrayEData[i+363] = timeStringaXMD5[i];
  unsigned char* hash=MD5::make_hash(arrayEData);
  char *md5str = MD5::make_digest(hash, 16);
  free(hash);
  String stringToReturn = md5str;
  free(md5str);
  return (stringToReturn);  
  
}

/*___________________________________________________________________________
saveEeprom2SD
salva su SD le impostazioni di setup memorizzate in eeprom
*/ 
void saveEeprom2SD()
{
  if (SD.exists("EEPROM")) SD.remove("EEPROM");
  dataEeprom =SD.open("EEPROM", FILE_WRITE); 
  for (int i=0; i<350; i++)
     dataEeprom.write(EEPROM.read(i));
  dataEeprom.flush();
  dataEeprom.close();
}

/*___________________________________________________________________________
loadSD2Eeprom
carica i valori di setup in eeprom prelevandoli da SD
*/ 
void loadSD2Eeprom()
{ byte c;
  Serial.println("carica da sd su eeprom");
  if (SD.exists("EEPROM"))
  { 
  dataEeprom =SD.open("EEPROM", FILE_READ);
  for (int i=0; i<350; i++)
     {c = dataEeprom.read();
      EEPROM.write(i, c) ;
     }
  dataEeprom.close();
  }
}

/*___________________________________________________________________________
setWDT
setta i registri interni per watchdog
*/ 
void setWDT(byte b)
{
  SREG &= ~(1 << SREG_I); 
  MCUSR = 0;
  WDTCSR |= ((1 << WDCE) | (1 << WDE));
  WDTCSR = b;
  SREG |= (1 << SREG_I); 
}

/*___________________________________________________________________________
ISR
avviso per ISR
*/ 
ISR (WDT_vect)
{ Serial.println("in ISR!!");
}

