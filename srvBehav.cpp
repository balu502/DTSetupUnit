#include <QThread>
//#include <w32api.h>
#define WINVER WindowsXP
#define ES_AWAYMODE_REQUIRED 0x00000040
//#include <windows.h>
//#include <winbase.h>
#include "srvBehav.h"
//#include "qexpodialog.h"

#include <QVBoxLayout>
#include <QDomDocument>
#include <QDataStream>

#define init_buff( BBB, SSS ) {for(int j=0; j<SSS; j++)  BBB[j] = '\0';}
#define averageDouble( VVV ){average( std::vector<double>( VVV.begin(), VVV.end() )  )}
#define maxInt( VVV ) (*std::max_element( VVV.begin(), VVV.end() ))

QString get_link_url(QString fn);

//-----------------------------------------------------------------------------
//--- Constructor
//-----------------------------------------------------------------------------
//TServer_Dt::TServer_Dt(QString dName,int nPort, QWidget *parent)
//    : QMainWindow(parent)

TServer_Dt::TServer_Dt(QWidget *parent)
    : QMainWindow(parent)
{
// Stay for example. On windows platfor don't work becouse change LIB environment variable but windows
// work with PATCH variable
// QCoreApplication::addLibraryPath(QString("C:/DtXml"));
// QCoreApplication::addLibraryPath(QString("C:/Program1/QtSDK/Desktop/Qt/4.7.4/mingw/plugins"));
// qDebug()<<QCoreApplication::libraryPaths();
// qputenv("PATH", "C:\\DtXml\\"); // change PATH in issue proceses!!!
  QString compilationTime = QString("%1 %2").arg(__DATE__).arg(__TIME__);
 // qDebug()<<compilationTime;

//  PORT = nPort;
//  NAME = dName;
  HWIN=(HWND)this->winId(); // set window ID
//  bHWIN=QString(" WinID=%1 ").arg((int)HWIN,0,16).toAscii();
  bHWIN=QString(" WinID=%1 ").arg((int)HWIN,0,16).toLatin1();
  device=0;
  //sHWIN=QString("%1").arg(HWIN);
  logSys=new LogObject("system"); logSys->setLogEnable(false,true); // enable system log into file only
  logSys->logMessage(tr(bHWIN+"Start Dt-server. Ver. "+APP_VERSION)+" "+compilationTime);
//  logSys->logMessage(bHWIN+"Device name "+NAME+QString(" Port=%1").arg(PORT));
//  setTrayIconActions();
//  showTrayIcon();

  //Qt::WindowFlags flags = Qt::Window |  Qt::CustomizeWindowHint| Qt::WindowSystemMenuHint |
  //                        Qt::WindowMinimizeButtonHint | Qt::WindowStaysOnTopHint;
//  Qt::WindowFlags flags = Qt::Window
//          | Qt::WindowSystemMenuHint
//          | Qt::WindowCloseButtonHint
//          | Qt::WindowStaysOnTopHint;
//  setWindowFlags(flags);


// read settings
  QString exeDir = qApp->applicationDirPath();
  setingsFileName= exeDir+"/setup.ini" ;
  IniFileRead();

// create pages with log and settings
  pageSrvD= new TPageLog;
  pageNWD=  new TPageLog;
  pageDTD=  new TPageLog;
  pageSetup=new TPageSetup;
  pageSpectral = new TPageTableInput();
  pageOptic = new TPageTableInput;
  pageExpos = new TPageTableInput;

// create tab widget
  tabWidget=new QTabWidget;
  tabWidget->addTab(pageSrvD, tr("Sys.diag"));
//  tabWidget->addTab(pageNWD, tr("Net.diag"));
  tabWidget->addTab(pageDTD, tr("DT-XX.diag"));
//  tabWidget->addTab(pageSetup, tr("Setup "));
  tabWidget->addTab(pageSpectral, tr("Spectral"));
  tabWidget->addTab(pageOptic, tr("Optic"));
  tabWidget->addTab(pageExpos, tr("Expos"));

  pageExpos->table->setHorizontalHeaderLabels(
        QStringList()<< tr("Exposition") );

  pageSpectral->table->setHorizontalHeaderLabels(
        QStringList()<< tr("Chan")<< tr("Group")
              << QString(":").repeated(COUNT_CH).split(":") );

  pageOptic->table->setHorizontalHeaderLabels(
        QStringList()
              << QString(":").repeated(COUNT_CH).split(":") );


  QWidget       *mw      = new QWidget;
  QWidget       *ew      = new QWidget;
  QVBoxLayout   *vLayout = new QVBoxLayout;
  QHBoxLayout   *eLayout = new QHBoxLayout;
                 cmbxExs = new QComboBox();

  eLayout->addWidget( cmbxExs );

  ew->setLayout( eLayout );

  vLayout->addWidget( ew );
  vLayout->addWidget( tabWidget );

  mw->setLayout( vLayout );
  setCentralWidget( mw );

  tabWidget->setCurrentIndex(0);

  setupUI();

  QString efn = exeDir +"/expo.txt";
  QString sfn = exeDir +"/service.txt";

  if ( QFile( sfn ).exists() )
      m_httpServiceUrl = get_link_url( sfn );

  if (m_httpServiceUrl.isEmpty())
      m_httpServiceUrl = "http://192.168.0.167:8080/dtxp/ticket.file";
//    m_httpServiceUrl = "http://service.dna-technology.ru/dtxp/ticket.file";
//    m_httpServiceUrl = "http://127.0.0.1/dtxp/ticket.file";
//    m_httpServiceUrl = "http://172.16.0.1:8080/dtxp/ticket.file";

  connect(pageExpos->commands->button(QDialogButtonBox::Retry),
            SIGNAL(clicked(bool)), this, SLOT(readCurrentExpo()));
  connect(pageExpos->commands->button(QDialogButtonBox::Apply),
            SIGNAL(clicked(bool)), this, SLOT(inputNewExpo()));

  connect(pageSpectral->commands->button(QDialogButtonBox::Retry),
            SIGNAL(clicked(bool)), this, SLOT(readCurrentSpectral()));
  connect(pageSpectral->commands->button(QDialogButtonBox::Apply),
            SIGNAL(clicked(bool)), this, SLOT(writeSpectral()));
  connect(pageSpectral->commands->button(QDialogButtonBox::Open),
            SIGNAL(clicked(bool)), this, SLOT(importSpectral()));

  connect(pageOptic->commands->button(QDialogButtonBox::Retry),
            SIGNAL(clicked(bool)), this, SLOT(readCurrentOptics()));
  connect(pageOptic->commands->button(QDialogButtonBox::Apply),
            SIGNAL(clicked(bool)), this, SLOT(writeOptics()));
  connect(pageOptic->commands->button(QDialogButtonBox::Open),
            SIGNAL(clicked(bool)), this, SLOT(importOptics()));


  connect( &m_WebCtrl, SIGNAL(finished(QNetworkReply*))
                        , this, SLOT(httpTicketDownloaded(QNetworkReply*)));

  if( !(authOk = QFile( efn ).exists()) )
    httpTicketLoad();

  device = new TDtBehav(HWIN,log_Size,log_Count);

  if( device->readGlobalErr() ){ //connect to network error. Program abort
    logSys->logMessage(bHWIN+device->readGlobalErrText());
    QTimer::singleShot(1000,this,SLOT(close()));
    return;
  }

  setLogEnable();
  cmbxExs->addItem( "Connecting..." );

//  connect(device,SIGNAL(commError()),this,SLOT(quiteProg()));
// signals for Log pages
  connect(device->logSrv, SIGNAL(logScrMessage(QString)),pageSrvD, SLOT(addToLog(QString)));
  connect(device->logNw,  SIGNAL(logScrMessage(QString)),pageNWD,  SLOT(addToLog(QString)));
  connect(device->logDev, SIGNAL(logScrMessage(QString)),pageDTD,  SLOT(addToLog(QString)));
  connect(pageNWD,        SIGNAL(pauseChange(bool)),     pageSetup,SLOT(setCbLogNWS(bool)));

//  connect(device, SIGNAL(onDeviceInfoAvailable(bool)), pageSpectral,SLOT(setEnabled(bool)));
//  connect(device, SIGNAL(onDeviceInfoAvailable(bool)), pageOptic,   SLOT(setEnabled(bool)));
//  connect(device, SIGNAL(onDeviceInfoAvailable(bool)), pageExpos,   SLOT(setEnabled(bool)));

  connect(device, SIGNAL(onDeviceInfoAvailable(bool)),  this, SLOT(onDeviceConnectionChanged(bool)));
  connect(device, SIGNAL(onDeviceListAvailable()),      this, SLOT(onDeviceListChanged()));

  connect(pageDTD,SIGNAL(pauseChange(bool)),pageSetup,SLOT(setCbLogDTS(bool)));
// signal on change data in setup page
  connect(pageSetup,SIGNAL(sigChangeUI()),this,SLOT(getUI())); //change combo box on page settings
  //device->start(QThread::NormalPriority);
  setMinimumWidth(500);
  resize(500,330);
  move(50, 50);
  SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED);
  device->logSrv->logMessage(tr("Start Dt-server. Ver. ")+APP_VERSION+" "+compilationTime);
  device->start(QThread::NormalPriority);

}

//-----------------------------------------------------------------------------
//--- Destructor
//-----------------------------------------------------------------------------

TServer_Dt::~TServer_Dt(){

  qDebug()<<"Server destructor start.";
  if(device) delete device;
  delete tabWidget;
//  delete trayIcon;

  logSys->logMessage(tr(bHWIN+"Stop Dt-server."));
  if(logSys) delete logSys;
  qDebug()<<"Server destructor finished.";
}


void TServer_Dt::onDeviceConnectionChanged(bool s){

    qDebug() << "Device is" << (s?"open":"closed");

    cmbxExs->clear();
    devControllersVersion.clear();

    QDialogButtonBox* bb;
    QString serD;

    bb = pageSpectral->commands;
    bb->button(QDialogButtonBox::Retry)->setEnabled(s);
    bb->button(QDialogButtonBox::Apply)->setEnabled(s);

    bb = pageOptic->commands;
    bb->button(QDialogButtonBox::Retry)->setEnabled(s);
    bb->button(QDialogButtonBox::Apply)->setEnabled(s);

    bb = pageExpos->commands;
    bb->button(QDialogButtonBox::Retry)->setEnabled(s);
    bb->button(QDialogButtonBox::Apply)->setEnabled(s);

    if (!s
    ||  (serD = device->property("serial").toString()).isEmpty() ) {
        cmbxExs->addItem( "Device disconnected" );
        return;
    }

    cmbxExs->addItem( QString("%1 is online").arg(serD) );
    setWindowTitle( serD );

    foreach( QString contrInfo
             , device->infoDevice(INFODEV_version).split("\r\n") ){

        setVersionController( contrInfo );
    }
}


void TServer_Dt::onDeviceListChanged(){

    bool ok;
    QStringList allDevs =
                    device->availableDevices();

    qDebug() << "DevList" << allDevs;

    if (allDevs.empty())
        return;

    if (1 == allDevs.count())
        return device->selectDevice( allDevs.first() );

    QString devName = QInputDialog::getItem(
                            this, this->windowTitle()
                            , "Choose device", allDevs
                            , 0, false, &ok );
    device->selectDevice(
        ok? devName: allDevs.first() );
}


void TServer_Dt::httpTicketLoad(){

    cmbxExs->clear();
    cmbxExs->addItem( "Waiting for network..." );
    m_WebCtrl.get(
        QNetworkRequest(QUrl(m_httpServiceUrl)));
}

typedef struct
{
    char    id_default[  8];             // "default"
    short   Expo_def  [ 16];             //
    char    reserve   [472];

} Device_ParDefault;


QStringList processReadExpo(QString sdata){

    QByteArray       buf_default;
    QString          str_default;
    QStringList      retExp;
    short            ex;

    union{
        Device_ParDefault   Device_ParamDefault;
        unsigned char       byte_buf[ 512 ];
    } DEVICE_PAR_DEFAULT;


    if ( sdata.isEmpty() )
        return  retExp;

    buf_default = QByteArray::fromBase64(
                        sdata.toStdString().c_str() );

    if ( buf_default.size() != 512 )
        return  retExp;

    for( int i=0; i<512; i++ )
        DEVICE_PAR_DEFAULT.byte_buf[i]
                            = buf_default.at(i);
    str_default = QString(
        DEVICE_PAR_DEFAULT.Device_ParamDefault.id_default );

    if ( !str_default.contains("default") )
        return  retExp;


    for ( int j=0; j<COUNT_CH*2; j++ ){
        ex = DEVICE_PAR_DEFAULT.Device_ParamDefault.Expo_def[ j ];

        if (0 == j%2)
            retExp << QString("%1").arg( ex );
    }

    return  retExp;
}


void TServer_Dt::gpgDecode(QByteArray dataFile){

    authOk = false;

    QProcess gpgProc;
    QString  phrase = QInputDialog::getText(
                        this, this->windowTitle()
                            , "Enter code", QLineEdit::Password );

    gpgProc.start( "gpg", QStringList()
        << "--batch" << "--passphrase" << phrase << "--" );

    if (!gpgProc.waitForStarted()){
        cmbxExs->addItem( "Err: gpg start" );
        cmbxExs->showPopup();
        return;
    }

    gpgProc.write(dataFile);
    gpgProc.closeWriteChannel();

    if (!gpgProc.waitForFinished()){
        cmbxExs->addItem( "Err: gpg result" );
        cmbxExs->showPopup();
        return;
    }

    if (QProcess::NormalExit != gpgProc.exitCode()){
        cmbxExs->addItem( "Err: gpg exit" );
        cmbxExs->showPopup();
        return;
    }

    QByteArray result = gpgProc.readAll();
    cmbxExs->addItem( QString("Out: %1").arg( result.length() ) );

    //qDebug()<<"Ticket:"<<result;
    authOk = true;
}


QStringList TServer_Dt::getCurrentExpo(){

    QByteArray  buf;
    QString     text;
    QString     ctrl = QString("PRDS %1").arg(0x0418);

     buf.resize(BYTES_IN_SECTOR);
    text.resize(BYTES_IN_SECTOR);

    if (!device->readFromUSB512( ctrl, "0", (unsigned char*)buf.data())){
        cmbxExs->addItem( "Err: current expo read" );
        cmbxExs->showPopup();
        return  QStringList();
    }

    text =  buf.toBase64();
            buf.clear();

    return  processReadExpo(text);
}


void TServer_Dt::readCurrentExpo(){

    QStringList currEx = getCurrentExpo();

    QTableWidget *t = pageExpos->table;

    t->clearContents();
    t->setColumnCount(1);
    t->setRowCount(currEx.count());

    int r=0;

    foreach( QString e, currEx){
        t->setItem( r++, 0, new QTableWidgetItem(e));
    }
}


bool parseSpectral(QByteArray btext, unionSpectralCoeff &ucs){

    int i,j,k;

    for(i=0; i<512; i++)  ucs.byte_buf[i] = btext[i];

    int count = ucs.SpectralCOEFF.count;
    bool  ret = true;
    QString answer, str;


    if(count < COUNT_CH || count >= 16){
        count = COUNT_CH;
        ret = false;
    }


    for(j=0; j<count; j++){

        QString fluor_name  = ucs.SpectralCOEFF.Info_Spectral[j].fluor_name;
        int id_group        = ucs.SpectralCOEFF.Info_Spectral[j].id_group;
        double value[MAX_OPTCHAN] = {0.000};

        if(id_group < 0 || id_group >= MAX_OPTCHAN) {ret = false; /*break;*/}

        answer= QString::asprintf("%s %d\t", fluor_name.toStdString().c_str(), id_group);

        for(k=0; k<COUNT_CH; k++){

            value[k] = ucs.SpectralCOEFF.Info_Spectral[j].coeff[k];
            value[k] = ((double)((int)(value[k])))/1000.;
            if(value[k] < 0 || value[k] > 2) {ret = false; /*break;*/}

            str = QString::asprintf(" %5.3lf", value[k]);
            answer += str;
        }
     }

    return ret;
}


void TServer_Dt::readCurrentSpectral(){

    if (pageSpectral->isDataUploaded()
    || (QGuiApplication::keyboardModifiers() & Qt::ControlModifier)){

        qDebug() << "Need to reload info";
        device->getInfoDevice();
        pageSpectral->setDataUploaded(false);
           pageOptic->setDataUploaded(false);
    }

    QString sk = device->infoDevice( INFODEV_SpectralCoeff );
    QByteArray text = QByteArray::fromBase64( sk.toStdString().c_str() );

    unionSpectralCoeff ucs;
    parseSpectral(text, ucs);

    QTableWidget *t = pageSpectral->table;
    Fluor_SpectralInfo *ifs;

    t->clearContents();
    t->setColumnCount(COUNT_CH + 2);
    t->setRowCount(ucs.SpectralCOEFF.count);

    QTableWidgetItem *iw;

    for(int j=0; j<ucs.SpectralCOEFF.count; j++){

        ifs = &(ucs.SpectralCOEFF.Info_Spectral[j]);

        t->setItem(j, 0, new QTableWidgetItem(ifs->fluor_name));
        t->setItem(j, 1, new QTableWidgetItem( QString("%1").arg(ifs->id_group) ));

        for(int k=0; k<COUNT_CH; k++){
            t->setItem(j, 2+k,
                       iw = new QTableWidgetItem(QString("%1").arg(
                             ((double) qRound((double)ifs->coeff[k])/10.)/100.)));
//                            ((double)((int)(ifs->coeff[k])))/1000.)));
//            qDebug() << ifs->coeff[k] << "-->" << iw->text();
        }
    }

    return;
}


bool TServer_Dt::writeSpectral(){

    QTableWidget *t = pageSpectral->table;
    int     r, c, count = t->rowCount();
    bool    ret = true;
    double  cVal;

    QString ctrl = "CWRS 0";
    QString str_ret, fluor_name;

    unionSpectralCoeff SPECTRAL_COEFF;
    Fluor_SpectralInfo *isp;

    init_buff( SPECTRAL_COEFF.byte_buf, 512 );
    SPECTRAL_COEFF.SpectralCOEFF.count = count;

    for(r=0; (r<count) && ret; r++){

        isp         = &SPECTRAL_COEFF.SpectralCOEFF.Info_Spectral[r];
        fluor_name  = t->item(r, 0)->text().left(11);
        isp->id_group=t->item(r, 1)->text().toInt(&ret);

        strcpy( isp->fluor_name, fluor_name.toStdString().c_str());

        for(c=0; (c<COUNT_CH) && ret; c++){
            cVal = qRound( 1000. * t->item(r, c+2)->text().toDouble(&ret) );
            isp->coeff[c] = (short) cVal;
        }
    }

    if (ret){
        ret = device->writeIntoUSB512( ctrl, "0", SPECTRAL_COEFF.byte_buf );
        ret ? cmbxExs->addItem( "Write: ok" ) : cmbxExs->addItem( "Err: data write" );
        pageSpectral->setDataUploaded(true);
    }else
        cmbxExs->addItem( "Icorrect input data" );

    cmbxExs->showPopup();

    return  ret;
}


bool TServer_Dt::importSpectral(QString fn){

    QString fileName = fn.isEmpty()
                ? QFileDialog::getOpenFileName(
                        this, tr("Open COR File"),
                        QString(), tr("COR files (*.cor);; RealTime files (*.rt)"))
                : fn;

    if (fileName.isEmpty())
        return false;
    else if (fileName.right(3).toLower()==".rt")
        return calcSpectral(fileName);


    QTableWidget *t = pageSpectral->table;
    QFile fileIn(fileName);

    if (!fileIn.open(QIODevice::ReadOnly | QIODevice::Text)){
        cmbxExs->addItem( "Err: file read error" );
        cmbxExs->showPopup();
        return false;
    }

    QList< QStringList > arr;
    QTextStream stream(&fileIn);
    QStringList strVals;
    QRegExp     rsep("\\s+");
    QString     s;
    bool        okV;
    double      val;
    int     countF=0, r=0, c;

    while (stream.readLineInto(&s)) {

        strVals = s.trimmed().split(rsep, QString::SkipEmptyParts);
        if(s.trimmed().isEmpty())break;

        countF = qMin(strVals.count(),
                         countF? countF: (2+COUNT_CH));
        arr << strVals;
    }

    t->clearContents();
    t->setRowCount( arr.count() );
    t->setColumnCount(2+COUNT_CH);

    foreach( strVals, arr ){

        foreach( s, strVals ){

            if( c >= countF) break;

            val = s.toDouble(&okV);
            if(!c || okV) t->setItem(r, c++, new QTableWidgetItem(s));
        }
        r++; c=0;
    }

    return true;
}



typedef QList<unionOpticalCoeff> ListCoefOpticals;

bool parseOptical(QByteArray btext, ListCoefOpticals& ops, int numTubes){

    int c, i, b=0;
    QStringList s;

    for(c=0; c<COUNT_CH; c++)  {

        unionOpticalCoeff uops;
        s.clear();

        for(i=0; i<512; i++, b++)
            uops.byte_buf[i] = btext[b];


        for (i=0; i < numTubes; i++)
            s << QString::number( uops.OpticalCOEFF.coeff[i] );

//        qDebug() << uops.OpticalCOEFF.fluor_name << s.join(" ");

        ops.append(uops);
    }

    return true;
}


void TServer_Dt::readCurrentOptics(){

    if (pageOptic->isDataUploaded()
    || (QGuiApplication::keyboardModifiers() & Qt::ControlModifier)){

        qDebug() << "Need to reload info";
        device->getInfoDevice();
        pageSpectral->setDataUploaded(false);
           pageOptic->setDataUploaded(false);
    }

    QString opk = device->infoDevice( INFODEV_OpticalCoeff );
    QByteArray text = QByteArray::fromBase64( opk.toStdString().c_str() );

    int t, ic=0, numt = device->property("tubes").toInt();

    ListCoefOpticals    uops;
    parseOptical(text,  uops, numt);

    QTableWidgetItem *iw;
    QTableWidget     *tbl = pageOptic->table;
    unionOpticalCoeff uo;

    tbl->clearContents();
    tbl->setColumnCount(COUNT_CH);
    tbl->setRowCount(numt);

    foreach(uo, uops){

        for (t=0; t< numt; t++){

            iw = new QTableWidgetItem( QString("%1").arg(
                  ((double) qRound((double)uo.OpticalCOEFF.coeff[t])/10.)/100.));

            tbl->setItem(t, ic, iw);
        }

        ic++;
    }

    return;
}


VersionInfo parseDefVer(QString s, QString &n){
    QStringList sparts = s.split(" ", QString::SkipEmptyParts);
    n = sparts[0].trimmed();
    return VersionInfo ( QVersionNumber::fromString(sparts[1]), sparts[2].trimmed() );
}

void TServer_Dt::setVersionController(QString a){

    if (a.trimmed().isEmpty())
        return;

    QString conName;
    VersionInfo conVer;

    conVer = parseDefVer(a, conName);

    if (!conName.isEmpty())
         devControllersVersion[ conName ] = conVer;

//    qDebug() << conName << "controller";
}


bool readMeasElement( QDomNode *n,
                      int* ixCh, int* ixNm, int* ixBk, int* ixEn, int* ixEv,
                      QList<int> *vals ){
    QDomNodeList tmp;
    QString     data;
    QDomElement  elt = n->toElement();

    if ((tmp = elt.elementsByTagName("optical_channel")).isEmpty())
        return false;
    *ixCh = tmp.item(0).toElement().text().toInt();

    if ((tmp = elt.elementsByTagName("fn")).isEmpty())
        return false;
    *ixNm = tmp.item(0).toElement().text().toInt();

    if ((tmp = elt.elementsByTagName("blk_exp")).isEmpty())
        return false;
    *ixBk = tmp.item(0).toElement().text().toInt();

    if ((tmp = elt.elementsByTagName("exp_number")).isEmpty())
        return false;
    *ixEn = tmp.item(0).toElement().text().toInt();

    if ((tmp = n->toElement().elementsByTagName("expVal")).isEmpty())
        return false;
    *ixEv = tmp.item(0).toElement().text().toInt();

    if ((tmp = elt.elementsByTagName("data")).isEmpty())
        return false;

    data = tmp.item(0).toElement().text().trimmed();

//    qDebug() << data;

    vals->clear();

    foreach(QString v, data.split(" "))
        (*vals) << v.toInt();

    return  true;
}

double average(std::vector<double> const& v){

    if(v.empty())
        return 0;

    auto const count = static_cast<double>(v.size());
    return std::accumulate(v.begin(), v.end(), 0.000) / count;
}

bool TServer_Dt::importRTFile(QString       fileName
                            , QHashMeas*    arrMeas
                            , QHashExpos*   arrExpo   ){

    QDomDocument doc("rt");
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly)){
        cmbxExs->addItem( "Err: file read error" );
        cmbxExs->showPopup();
        return false;
    }

    if (!doc.setContent(&file)) {
        cmbxExs->addItem( "Err: invalid file content" );
        cmbxExs->showPopup();
        file.close();
        return false;
    }

    file.close();

    QDomNodeList mEls = doc.elementsByTagName("measurements");
    QDomNode nextM;

    QList<int>       vs;    // Exposition values by tube, temporary list
    QPair<int, int> cfm;    // Pair - Channel x NumberMeas

    int  bgExp, OVERVAL = 0x1000;  // black value; value of threshold exposition
    int  useNumExp = -1;
    int  nExp, vExp, nCh, nMt;

    QHash< int, QHashMeas >  exposMeas;
    QHash< int, QHashExpos >valuesExpo;
    QHash< int, bool >     invalidExpo;

    if (mEls.count())
        mEls = mEls.at(0).toElement().elementsByTagName("item");

    int v;

    for ( int i=0; i < mEls.count(); i++ ){
        nextM = mEls.item(i);

        if (!readMeasElement(&nextM, &nCh, &nMt, &bgExp, &nExp, &vExp, &vs)){
            qDebug() << "Reading invalid meas";
            continue;
        }

        //qDebug() << nMt << nExp << nCh << vExp;

        cfm = qMakePair( nCh, nMt );
                                        // flag if value is overfloat by exposition
        invalidExpo[ nExp ] |= maxInt( vs ) > OVERVAL;
        valuesExpo [ nExp ][ nCh ]  = vExp;

        for(v=0; v < vs.count(); v++)
            vs[v] = (vs[v]%OVERVAL - bgExp);

        exposMeas[ nExp ][ cfm ] = vs;
    }

    doc.clear();

    while( invalidExpo.count() )
        if ( !invalidExpo.take( ++useNumExp ) ) break;

    qDebug() << "Using expo index" << useNumExp;

    if ( arrExpo ){
        *arrExpo = valuesExpo[ useNumExp ];
//        qDebug() << arrExpo->keys();
//        qDebug() << arrExpo->values();
    }

    *arrMeas = exposMeas[ useNumExp ];

    return true;
}


bool TServer_Dt::calcSpectral( QString fileName ){

    QFileInfo infoFile( fileName );
    QDir srcDir(infoFile.absolutePath());
    QStringList filesSet;

    QFileInfoList list = srcDir.entryInfoList();
    QHashMeas   *arrMeas;
    QHashExpos  *arrExpos;
    QHash <int, QPair<double, double>> chSpec;

    bool  okI = true;

    qDebug()<< "Using files set";

    for (int i = 0; okI && (i < list.size()); ++i) {

        infoFile = list.at(i);

        if (( infoFile.suffix().toLower() == "rt" )
        &&  ( infoFile.fileName().left(6).toLower() == "cross_" )){
            filesSet << infoFile.filePath();

            arrExpos= new QHashExpos();
            arrMeas = new QHashMeas();

            QSpecChannel specs;

            if ((okI = importRTFile(infoFile.filePath(), arrMeas, arrExpos))){

                  qDebug() << infoFile.fileName() << arrMeas->count() << "series";

                  okI = splitChannelsSpectral(arrMeas, arrExpos, &specs);

                  if ( okI )
                      chSpec[ specs.first ] = specs.second;

//                  QStringList s; int c;
//                  foreach( c, arrExpos->keys() )
//                      s << QString("%1(%2)").arg(c).arg( arrExpos->value(c) );
//                  qDebug() << "Expos: " << s.join("; ");

            }else qDebug() << infoFile.fileName() << "broaken";

            delete arrMeas; delete arrExpos;
        }
    }

    if ( !okI )
        return false;


    QTableWidget *t = pageSpectral->table;

    t->clearContents();
    t->setColumnCount(  chSpec.count() + 2);
    t->setRowCount(     chSpec.count()  );

    QStringList fluorName = QString( "Fam Hex Rox Cy5 Cy5.5" ).split(" ");

    foreach (int i, chSpec.keys() ){
        qDebug() << i << chSpec[i];

        for (int r=0; r<t->rowCount(); r++)
            t->setItem( r, i+2, new QTableWidgetItem(QString::number(0.000, 'f', 3)));

        t->setItem( i, 0, new QTableWidgetItem(fluorName[ i ]));
        t->setItem( i, 1, new QTableWidgetItem(QString::number(i)));
        t->setItem( i,i+2,new QTableWidgetItem("1"));

        if ( i > 0 )
        t->setItem( i-1, i+2, new QTableWidgetItem(QString::number( chSpec[i].first, 'f', 3  )));

        if ( i < (t->rowCount()-1) )
        t->setItem( i+1, i+2, new QTableWidgetItem(QString::number( chSpec[i].second, 'f', 3 )));
    }


    return true;
}


bool TServer_Dt::splitChannelsSpectral(QHashMeas* arrMeas, QHashExpos* arrExpos, QSpecChannel *specs ){

    int countMeas = -1,
        countTubes = 0,
        t, c, m;

    QPair <int, int> mk;

    if (arrMeas->isEmpty())
        return false;

    countTubes = arrMeas->value(
                    arrMeas->keys().first() ).count();

    foreach( mk, arrMeas->keys() )
            countMeas = qMax( mk.second, countMeas );

    countMeas++;

    QHash< int, QList<int> > diffSumm;
    QHash< int, QList<int> > diffAvg;

    foreach( mk, arrMeas->keys() ){

        c =  mk.first;
        m = (mk.second*2 >= countMeas) ? -1: 1;

        if (!diffSumm.keys().contains( c )){
             diffSumm[ c ] = QList<int>();
             for ( t=0; t < countTubes; t++ )
                  diffSumm[ c ] << 0;
        }

        for ( t=0; t < countTubes; t++ )
             diffSumm[ c ][ t ] += m * arrMeas->value( mk )[ t ];        
    }


    foreach(c, diffSumm.keys()){

        diffAvg[c] = QList<int>();

        for ( t=0; t < countTubes; t++ )
            diffAvg[c] << qRound( qAbs( 2. * (double) diffSumm[ c ][ t ] / (double) countMeas ) );

        qDebug() << c;
//        qDebug() << diffAvg[c];
    }


    if ( 2 == arrExpos->count() ){

        c = arrExpos->keys().contains( 0 )
                ? -1 : 1 + qMax( arrExpos->keys()[0], arrExpos->keys()[1] );

        arrExpos->insert(c, 1);
        diffAvg[c] = QList<int>();

        for ( t=0; t < countTubes; t++ )
            diffAvg[c] << 1;

        qDebug() << "+" << c << "channel";
    }

    QList<int> cxSort = arrExpos->keys();
    std::sort( cxSort.begin(), cxSort.end() );
    double  k01=0, k21=0, ex0, ex1, ex2;

    for ( t=0; t < countTubes; t++ ){
        k01 += (double)diffAvg[cxSort[0]][t] / (double)diffAvg[cxSort[1]][t];
        k21 += (double)diffAvg[cxSort[2]][t] / (double)diffAvg[cxSort[1]][t];
    }

    ex0 = arrExpos->value( cxSort[0] );
    ex1 = arrExpos->value( cxSort[1] );
    ex2 = arrExpos->value( cxSort[2] );

    k01 = (k01/countTubes) * ex1 / ex0;
    k21 = (k21/countTubes) * ex1 / ex2;

    qDebug() << "KKK" << k01 << k21 << "|" << ex0 << ex1 << ex2 ;

    (*specs).first = cxSort[1];
    (*specs).second= qMakePair( k01, k21 );

    return true;
}


bool TServer_Dt::calcOptics( QString fileName ){

    QHashMeas   *arrMeas = new QHashMeas();

    if (!importRTFile(fileName, arrMeas)){
        delete arrMeas;
        return false;
    }

    QHash< int, QList< QPair< int, double > > >
            avgByChannel; // Average tubes value by each Channel

    QList< QPair<int, int> >
            measIx = arrMeas->keys();

    int  cntTubes = (arrMeas->count()? arrMeas->value( measIx.first() ).count(): 0);

    if ( cntTubes && !(cntTubes%48) ){

        QPair<int, int> chan_meas;  // iterator Channel x Measurement
        QList<int>      numMeas;
        QList<double>   meas;

        int xT=0, xCh=0;

        foreach( chan_meas, measIx ){
            if ( !avgByChannel.keys().contains( chan_meas.first ) )
                  avgByChannel[ chan_meas.first ] = QList< QPair< int, double > > ();

            if ( !numMeas.contains( chan_meas.second ) )
                  numMeas << chan_meas.second;
        }

        double  avgVal;
        QList<int> chIndex = avgByChannel.keys();
        std::sort( chIndex.begin(), chIndex.end() );

        foreach( xCh, chIndex ){

            for( xT=0; xT< cntTubes; xT++ ){

                meas.clear();

                foreach( int m, numMeas ){

                    chan_meas.first = xCh;
                    chan_meas.second= m;

                    if( measIx.contains( chan_meas ) ){

//                        if ( selM->value( chan_meas ).count() < cntTubes )
//                             meas << (double) selM->value( chan_meas ).at( xT );
//                        else meas << 0.0;

//                        meas << (double) selM->value( chan_meas ).at( xT );
                        meas << (double) arrMeas->value( chan_meas ).at( xT );
                    }
                }

                avgVal = averageDouble( meas );
                avgByChannel[ xCh ] << QPair< int, double >( xT, avgVal );

//              qDebug() << "xCh, xT" << xCh << xT << avgVal << "\n" << meas;
            }
        }

        QStringList svs; svs.clear();
        QPair< int, double > tubAvg;
        QHash< int, double > avgsTotal;

        QTableWidget *t = pageOptic->table;
        t->clearContents();
        t->setColumnCount( chIndex.count() );
        t->setRowCount( cntTubes );
        int h=0, v=0;

        foreach(xCh, chIndex){

            meas.clear();
            svs.clear();

            svs << QString::number(xCh) << ":";

            foreach( tubAvg, avgByChannel[xCh] )
                meas<< tubAvg.second;

            avgsTotal[ xCh ]= averageDouble( meas );

            foreach( tubAvg,  avgByChannel[ xCh ] ){
                tubAvg.second /= avgsTotal[ xCh ];
                svs << QString::number( tubAvg.second, 'f', 3 );

                t->setItem( h++,  v,
                    new QTableWidgetItem(
                            QString::number( tubAvg.second, 'f', 3 ) ) );
            }

            v++; h=0;
            qDebug() << svs.join(" ") << "\n"
                     << avgsTotal[ xCh ] << "\n";            
        }

    }else
        qDebug() << "Bad tubes count" << cntTubes;

    delete arrMeas;

    return true;
}


bool TServer_Dt::importOptics(QString fn){

    QString fileName = fn.isEmpty()
                ? QFileDialog::getOpenFileName(
                        this, tr("Open CRS File"),
                        QString(), tr("CRS files (*.crs);; RealTime files (*.rt)"))
                : fn;

    if (fileName.isEmpty())
        return false;
    else if (fileName.right(3)==".rt")
        return calcOptics(fileName);

    QTableWidget *t = pageOptic->table;
    QFile fileIn(fileName);

    if (!fileIn.open(QIODevice::ReadOnly | QIODevice::Text)){
        cmbxExs->addItem( "Err: file read error" );
        cmbxExs->showPopup();
        return false;
    }

    QList< QStringList > arr;
    QTextStream stream(&fileIn);
    QStringList strVals;
    QRegExp     rsep("\\s+");
    QString     s;
    bool        okV;
    double      val;
    int     countChannels=0, r=0, c;

    while (stream.readLineInto(&s)) {

        strVals = s.split(rsep, QString::SkipEmptyParts);
        arr << strVals;

        if(!countChannels){

            if(countChannels>COUNT_CH){
                cmbxExs->addItem( "Err: bad file format" );
                cmbxExs->showPopup();
                return false;
            }
            countChannels = strVals.count();
        }
    }

    t->clearContents();
    t->setColumnCount( COUNT_CH );
    t->setRowCount( arr.count() );

    foreach( strVals, arr ){
        foreach( s, strVals ){
            val = s.toDouble(&okV);
            if(okV) t->setItem(r, c++, new QTableWidgetItem(s));
        }
        r++; c=0;
    }

    return true;
}


bool TServer_Dt::writeOptics(){

    QTableWidget *t = pageOptic->table;
    int     count = t->rowCount();

    int     count_block= device->property("count_block").toInt();
    int     COUNT_TUBES= device->property("tubes").toInt();

    int     i,j,m;
    double  Value;
    QHash <int, double> value;

    QString ts, ctrl = "CWRS";
    bool    ret = true ;

    if ( count != COUNT_TUBES ){
        cmbxExs->addItem( QString("Err: invalid number tubes!") );
        cmbxExs->showPopup();
        return  ret=false;
    }

    for (i=0, ts=""; (i<count)&& ret; i++){
        for (j=0; (j<COUNT_CH)&& ret; j++){

            Value = t->item(i, j)->text().toDouble(&ret);
            value[i + j*COUNT_TUBES] = qRound(1000.* Value);
//            ts += " " + QString::number( (int) qRound(1000.* Value) );
        }
//        qDebug() << QString("%1, %2").arg(i).arg(ts);
    }

    if ( !ret ){
        cmbxExs->addItem( QString("Err: input invalid value!") );
        cmbxExs->showPopup();
        return  false;
    }

    count = qMin(count, 192);
    unionOpticalCoeff uo;

    for(m=0; (m<count_block)&& ret; m++){

        for(i=0, ts=""; (i<COUNT_CH)&& ret; i++){

            init_buff( uo.byte_buf, 512 );

            //for(j=0; j<192; j++){
            for(j=0; j<count; j++){

                uo.OpticalCOEFF.coeff[ j ] = (short)value[j + i*COUNT_TUBES + m*192];
                ts += " " + QString::number( uo.OpticalCOEFF.coeff[ j ] );
            }

//            qDebug() << QString("%1, %2").arg(i).arg(ts);
            ret = device->writeIntoUSB512(
                        QString("%1 %2").arg(ctrl).arg((i+1)*10 + m), "0", uo.byte_buf );
            Sleep(250);
        }
    }


    ret ? cmbxExs->addItem( "Write: ok" )
        : cmbxExs->addItem( "Err: data write" );

    cmbxExs->showPopup();

    pageOptic->setDataUploaded(true);


    return  ret;
}


void TServer_Dt::inputNewExpo(){

    if (!authOk){
        cmbxExs->clear();
        cmbxExs->addItem( QString("Err: auth") );
        cmbxExs->showPopup();

        httpTicketLoad();
        return;
    }

    QStringList chLines;
    QStringList currEx = getCurrentExpo();

    QTableWidget *t = pageExpos->table;
    int r=0; bool inOk;

    for (r=0; r<t->rowCount(); r++){
        chLines << QString::number( t->item(r, 0)->text().toInt(&inOk) );
        if (!inOk){
            qDebug() << QString("Invalid Expo[%1]='%2'").arg(r)
                                    .arg(t->item(r, 0)->text());
            return;
        }
    }

    if (QMessageBox::No
    ==  QMessageBox::question(this, this->windowTitle(),
            tr("%1Continue writing expo [%2] into device?")
                .arg( QString("Current expo: [%1].\n")
                        .arg(currEx.isEmpty()?"unavailable":currEx.join("; ") ) )
                .arg( chLines.join("; ") ),
            QMessageBox::Yes|QMessageBox::No, QMessageBox::No ))
        return;

    writeDeviceExpo( chLines );
}


void TServer_Dt::writeDeviceExpo( QStringList strExp ){

    QString ctrl = QString("PWRS %1").arg(0x0418);

    union{  Device_ParDefault   Device_ParamDefault;
            unsigned char       byte_buf[ 512 ];
    }   DEVICE_PAR_DEFAULT;

    for( int i=0; i<512; i++ )
        DEVICE_PAR_DEFAULT.byte_buf[i] = 0;

    strcpy( DEVICE_PAR_DEFAULT.Device_ParamDefault.id_default, "default" );
//    strcpy( DEVICE_PAR_DEFAULT.Device_ParamDefault.id_default, "undeflt" );

    short   ex;
    for(int i=0; i < COUNT_CH; i++){
        ex = ( i >= strExp.length() ) ? 0 : strExp[i].trimmed().toUShort();
        DEVICE_PAR_DEFAULT.Device_ParamDefault.Expo_def[2*i+0] = ex;
        DEVICE_PAR_DEFAULT.Device_ParamDefault.Expo_def[2*i+1] = ex/5;
    }

    if (!device->writeIntoUSB512( ctrl, "0", DEVICE_PAR_DEFAULT.byte_buf )){
        cmbxExs->addItem( "Err: data write" );
        cmbxExs->showPopup();
        return;
    }

    cmbxExs->addItem( "Write: ok" );
    cmbxExs->showPopup();

    readCurrentExpo();
}


void TServer_Dt::httpTicketDownloaded(QNetworkReply *pReply){

    QByteArray dataRep = pReply->readAll();
    gpgDecode( dataRep );
}


//-----------------------------------------------------------------------------
//--- Read ini settings
//-----------------------------------------------------------------------------
void TServer_Dt::IniFileRead(void)
{
  QSettings setings(setingsFileName,QSettings::IniFormat) ;
  logSrvScr=setings.value("logSystemScr",true).toBool();
  logSrv=setings.value("logSystem",true).toBool();
  logNWScr=setings.value("logNWScr",true).toBool();
  logNW=setings.value("logNW",true).toBool();
  logDTScr=setings.value("logDTScr",true).toBool();
  logDT=setings.value("logDT",true).toBool();
  logDTM=setings.value("logDTMASTER",false).toBool();
  log_Size=setings.value("logFileSize",100000).toInt();
  log_Count=setings.value("logFileCount",10).toInt();
  loc=setings.value("Locale","EN").toString();
}

//-----------------------------------------------------------------------------
//--- Write ini settings
//-----------------------------------------------------------------------------
void TServer_Dt::IniFileWrite(void)
{
   QSettings setings(setingsFileName,QSettings::IniFormat) ;

   setings.setValue("logSystemScr",logSrvScr);
   setings.setValue("logSystem",logSrv);
   setings.setValue("logNWScr",logNWScr);
   setings.setValue("logNW",logNW);
   setings.setValue("logDTScr",logDTScr);
   setings.setValue("logDT",logDT);
}

//-----------------------------------------------------------------------------
//--- Sets enable/disable log information in log object
//-----------------------------------------------------------------------------
void TServer_Dt::setLogEnable(void)
{
  device->logNw->setLogEnable(logNWScr,logNW);
  device->logSrv->setLogEnable(logSrvScr,logSrv);
  device->logDev->setLogEnable(logDTScr,logDT);
  device->logDTMaster->setLogEnable(false,logDTM);
}

//-----------------------------------------------------------------------------
//--- Put on user interface data from settings
//-----------------------------------------------------------------------------
void TServer_Dt::setupUI(void)
{
  pageSetup->setCbLogSysS(logSrvScr);
  pageSetup->setCbLogSys(logSrv);
  pageSetup->setCbLogNWS(logNWScr);
  pageSetup->setCbLogNW(logNW);
  pageSetup->setCbLogDTS(logDTScr);
  pageSetup->setCbLogDT(logDT);
}

//-----------------------------------------------------------------------------
//--- Private slot on change combo box in setup page
//-----------------------------------------------------------------------------
void TServer_Dt::getUI(void)
{
  logSrvScr=pageSetup->getCbLogSysS();
  logSrv=pageSetup->getCbLogSys();
  logNWScr=pageSetup->getCbLogNWS();
  logNW=pageSetup->getCbLogNW();
  logDTScr=pageSetup->getCbLogDTS();
  logDT=pageSetup->getCbLogDT();

  setLogEnable(); //update log
  IniFileWrite();
}

/*
//-----------------------------------------------------------------------------
//--- Private slot
//--- Show icon from resurces in tray
//-----------------------------------------------------------------------------
void TServer_Dt::showTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    QIcon trayImage = QIcon(":/image/usb_32.png");
    trayIcon->setIcon(trayImage);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip("DT-XX server");
    setWindowIcon(trayImage);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this,     SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    trayIcon ->show();
}

//-----------------------------------------------------------------------------
//--- Private slot
//--- Set actions in pull up menu
//-----------------------------------------------------------------------------
void TServer_Dt::setTrayIconActions()
{
  //... Setting actions ...
  minimizeAction = new QAction(tr("Minimize"), this);
  restoreAction = new QAction(tr("Restore"), this);
  quitAction = new QAction(tr("Exit"), this);

  //... Connecting actions to slots ...
  connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hideProg()));
  connect(restoreAction, SIGNAL(triggered()), this, SLOT(showProg()));
  connect(quitAction, SIGNAL(triggered()), this, SLOT(quiteProg()));


  //... Setting system tray's icon menu ...
  trayIconMenu = new QMenu(this);

  trayIconMenu->addAction (restoreAction);
  trayIconMenu->addAction (minimizeAction);
  trayIconMenu->addSeparator();
  trayIconMenu->addAction (quitAction);
}
*/

/*
//-----------------------------------------------------------------------------
//--- Private slot
//--- tray processing function. Reaction on press mouse in the icon region
//-----------------------------------------------------------------------------
void TServer_Dt::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
  switch (reason) {
  case QSystemTrayIcon::Trigger:
  case QSystemTrayIcon::DoubleClick:
    if(!isVisible()) this->showNormal(); else hide();
    break;
  case QSystemTrayIcon::MiddleClick:
    break;
  default:
    break;
  }
}
*/

// actions on tray menu pressed  private slots
//-----------------------------------------------------------------------------
//--- Private slot
//--- restore window
//-----------------------------------------------------------------------------
void TServer_Dt::showProg(void)
{
  showNormal();
}
//-----------------------------------------------------------------------------
//--- Private slot
//--- hide window
//-----------------------------------------------------------------------------
void TServer_Dt::hideProg(void)
{
  hide();
}

//-----------------------------------------------------------------------------
//--- Private slot
//--- quite from program
//-----------------------------------------------------------------------------
void TServer_Dt::quiteProg(void)
{
  qApp->quit();
}

/*
//-----------------------------------------------------------------------------
//--- Reimplement of the close window event
//-----------------------------------------------------------------------------
void TServer_Dt::closeEvent(QCloseEvent *event)
{
  if(trayIcon->isVisible()) {
    hide();//quiteProg();
    event->ignore();
  }

}
*/

QString get_link_url(QString fn){

    QString r;
    QFile   data(fn);

    if (!data.open(QFile::ReadOnly))
        return r;

    QTextStream out(&data);
    QString line;

    do {
        if ((line = out.readLine()).toUpper().startsWith("URL"))
            return  line.split("=")[1].trimmed();

    } while( !line.isNull() );

    return r;
}
