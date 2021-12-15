#include "pages.h"

//-----------------------------------------------------------------------------
//--- Constructor
//--- Pages system  network  device
//-----------------------------------------------------------------------------
TPageLog::TPageLog(QWidget *parent) : QWidget(parent)
{
  log=new QTextBrowser ;
  QVBoxLayout *mainLayout = new QVBoxLayout;

  log->clear();
  mainLayout->addWidget(log);
  log->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(log,  SIGNAL(customContextMenuRequested(const QPoint&)),
          this, SLOT(showContextMenu(const QPoint&)));
  setLayout(mainLayout);
  countLines=0;
}

//-----------------------------------------------------------------------------
//--- Destructor
//-----------------------------------------------------------------------------
TPageLog::~TPageLog()
{
  qDebug()<<"Log page destructor";
  if(log) delete log;
}

//-----------------------------------------------------------------------------
//--- Make user define menu on pages system,  network,  device
//-----------------------------------------------------------------------------
void TPageLog::showContextMenu(const QPoint& pt)
{
  QMenu * menu = log->createStandardContextMenu();

/* Example how create sub menu in menu
  QMenu * tags;
  tags=menu->addMenu(tr("&User menu"));
  QAction * taginstance = new QAction(tr("Clear"), this);
  connect(taginstance, SIGNAL(triggered()), log, SLOT(clear()));
  tags->addAction(taginstance);
*/
// add to menu actions
  QAction * clear = new QAction(tr("Clear"), this);
  connect(clear, SIGNAL(triggered()), log, SLOT(clear()));
  menu->addAction(clear);
  QAction * pause = new QAction(tr("Pause"), this);
  connect(pause, SIGNAL(triggered()), this, SLOT(clickPause()));
  menu->addAction(pause);
  QAction * run = new QAction(tr("Run"), this);
  connect(run, SIGNAL(triggered()), this, SLOT(clickRun()));
  menu->addAction(run);

  menu->exec(log->viewport()->mapToGlobal(pt));
  delete clear;
  delete pause;
  delete run;
  delete menu;
}

//-----------------------------------------------------------------------------
//--- Emit signal when press user menu pause
//-----------------------------------------------------------------------------
void TPageLog::clickPause()
{
  emit pauseChange(false);
}

//-----------------------------------------------------------------------------
//--- Emit signal when press user menu run
//-----------------------------------------------------------------------------
void TPageLog::clickRun()
{
  emit pauseChange(true);
}

//-----------------------------------------------------------------------------
//--- Constructor
//--- Pages Settings
//-----------------------------------------------------------------------------
TPageSetup::TPageSetup(QWidget *parent) : QWidget(parent)
{
  QGridLayout *mainLayout = new QGridLayout;

  cbLogSysS=new QCheckBox(tr("System log enable on screen"),this);
  cbLogSys=new QCheckBox(tr("System log enable into file"),this);
  cbLogNWS=new QCheckBox(tr("Network log enable on screen"),this);
  cbLogNW=new QCheckBox(tr("Network log enable into file"),this);
  cbLogDTS=new QCheckBox(tr("Device DT-XX log enable on screen"),this);
  cbLogDT=new QCheckBox(tr("Device DT-XX log enable into file"),this);

  connect(cbLogSysS,SIGNAL(clicked()),this,SIGNAL(sigChangeUI()));
  connect(cbLogSysS,SIGNAL(stateChanged(int)),this,SIGNAL(sigChangeUI()));
 // connect(cbLogSys,SIGNAL(clicked()),this,SIGNAL(sigChangeUI()));

  connect(cbLogNWS,SIGNAL(clicked()),this,SIGNAL(sigChangeUI()));
  connect(cbLogNWS,SIGNAL(stateChanged(int)),this,SIGNAL(sigChangeUI()));
  connect(cbLogNW,SIGNAL(clicked()),this,SIGNAL(sigChangeUI()));

  connect(cbLogDTS,SIGNAL(clicked()),this,SIGNAL(sigChangeUI()));
  connect(cbLogDTS,SIGNAL(stateChanged(int)),this,SIGNAL(sigChangeUI()));
  connect(cbLogDT,SIGNAL(clicked()),this,SIGNAL(sigChangeUI()));
  QString compilationTime = QString("SW %1 %2 %3").arg(APP_VERSION).arg(__DATE__).arg(__TIME__);
  labelSW =new QLabel(compilationTime);

  mainLayout->setSpacing(20);
  mainLayout->addWidget(cbLogSysS,0,0);
  mainLayout->addWidget(cbLogSys, 1,0);
  mainLayout->addWidget(cbLogNWS, 2,0);
  mainLayout->addWidget(cbLogNW,  3,0);
  mainLayout->addWidget(cbLogDTS, 4,0);
  mainLayout->addWidget(cbLogDT,  5,0);
  mainLayout->addWidget(labelSW,  6,0,Qt::AlignBottom);

  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------
//--- Destructor
//-----------------------------------------------------------------------------
TPageSetup::~TPageSetup()
{
  qDebug()<<"Settings page destructor";
}


TPageTableInput::TPageTableInput(QWidget *parent)
    : QWidget(parent)
    , _dataUpdated(false)
{
    table = new QTableWidget();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    commands = new QDialogButtonBox(  QDialogButtonBox::Retry
                                     |QDialogButtonBox::Open
                                     |QDialogButtonBox::Apply
                                      );

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(table);
    mainLayout->addWidget(commands);

    setLayout(mainLayout);

    commands->button(QDialogButtonBox::Retry)->setText(tr("Refresh"));
    commands->button(QDialogButtonBox::Open)->setText(tr("Import"));

    connect(commands->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(setDataUploaded()));

    QAction* pa = new QAction(this); pa->setShortcut(tr("Ctrl+C"));
    QAction* ca = new QAction(this); ca->setShortcut(tr("Ctrl+V"));

    connect(pa, SIGNAL(triggered()), this, SLOT(fillClipboardData()));
    connect(ca, SIGNAL(triggered()), this, SLOT(insertClipboardData()));

    table->addAction(pa);
    table->addAction(ca);
}

void TPageTableInput::fillClipboardData(){

    QTableWidgetSelectionRange
            sCells(0,0, table->rowCount(), table->columnCount());

    if ( table->selectedRanges().count() )
        sCells = table->selectedRanges().first();

    QString     s="";
    QStringList t;
    int     r, c;

    for (r=0; r< sCells.rowCount(); r++){
        for (c=0; c< sCells.columnCount(); c++)
            s += table->item(r+sCells.topRow(), c+sCells.leftColumn())->text() + " ";

        t << s.trimmed();
        s="";
    }

    QApplication::clipboard()->setText(t.join("\n"));
}


void TPageTableInput::insertClipboardData(){

    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData  *mimeData  = clipboard->mimeData();

    if (!mimeData->hasText())
        return;

    QList< QStringList > arr;
    QRegExp     rsep("\\s+");
    QStringList strVals;
    QString     s;
    int countCpFields=0, r, c;

    foreach(s, mimeData->text().split("\n")){
        arr << s.split(rsep, QString::SkipEmptyParts);

        countCpFields= countCpFields
                ? qMin(countCpFields, arr.last().count())
                : arr.last().count();
    }

    if(!table->rowCount())   table->setRowCount( arr.count() );
    if(!table->columnCount())table->setColumnCount( countCpFields );

    QTableWidgetSelectionRange
            sCells(0,0, table->rowCount(), table->columnCount());

    if ( table->selectedRanges().count() )
        sCells = table->selectedRanges().first();

    int FF = countCpFields,
        RR = arr.count();

    for (r=0; r< sCells.rowCount(); r++){
        for (c=0; c< sCells.columnCount(); c++){

            table->setItem( r+ sCells.topRow(), c+ sCells.leftColumn(),
                            new QTableWidgetItem( arr.at(r%RR).at(c%FF) ) );
        }
    }

}
