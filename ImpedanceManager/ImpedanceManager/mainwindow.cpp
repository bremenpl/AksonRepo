#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(const QString& fileToOpen, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    //Q_ASSERT(parent);

    ui->setupUi(this);
    initComponents();

    // connections:
    connect(this, SIGNAL(closeSerialThread()),
            mp_serialThread, SLOT(on_closePort()));

    connect(mp_serialThread, SIGNAL(openPort(const int&)),
            this, SLOT(at_mp_SerialThread_openPort(const int&)), Qt::UniqueConnection);

    connect(this, SIGNAL(send_getFirmwareID()),
            mp_serialThread, SLOT(on_send_getFirmwareID()), Qt::UniqueConnection);
    connect(mp_serialThread, SIGNAL(received_getFirmwareID(const MeasureUtility::union32_t&)),
            this, SLOT(at_received_getFirmwareID(const MeasureUtility::union32_t&)),
            Qt::UniqueConnection);

    connect(mp_serialThread, SIGNAL(rxTimeout(const int&)),
            this, SLOT(at_mp_SerialThread_rxTimeout(const int&)), Qt::UniqueConnection);

    this->setWindowState(Qt::WindowMaximized);

    if (fileToOpen != NULL)
    {
        openProject(fileToOpen);

        // update title
        setWindowTitle(APPNAME + getAppVersion() + " - " + fileToOpen);
    }
}

MainWindow::~MainWindow()
{
    delete ui;

    if (mp_serialThread)
    {
        mp_serialThread->quit();
        mp_serialThread->wait();
        mp_serialThread->deleteLater();
    }
}

QString MainWindow::getAppVersion()
{
    return QString("%1.%2.%3").arg(m_appVersion.ver8[2]).arg(m_appVersion.ver8[1]).arg(m_appVersion.ver8[0]) + " ";
}

void MainWindow::initComponents()
{
    qRegisterMetaType< MeasureUtility::union32_t >("MeasureUtility::union32_t");
    qRegisterMetaType< union32_t >("union32_t");
    qRegisterMetaType< MeasureUtility::EStepType_t >("MeasureUtility::EStepType_t");

    m_appVersion.ver8[2] = 1;         // Big new functionalities
    m_appVersion.ver8[1] = 5;         // new functionalities
    m_appVersion.ver8[0] = 1;         // changes to existing functionalities
    setWindowTitle(APPNAME + getAppVersion());

    QString settingsFile = QApplication::applicationDirPath() + "/settings.xms";
    CSettingsManager::instance()->setFilePath(settingsFile);
    setMachineState(EMachineState_t::eDisconnected);

    mp_serialThread = new CSerialThread(CSettingsManager::instance()->paramValue(XML_FIELD_PORT));
    mp_serialThread->moveToThread(mp_serialThread);

    checkCurrentTab(-1);
    mp_dummyProject = NULL;
}

CGenericProject* MainWindow::currentMeasObject(const int& index)
{
    auto measObj = dynamic_cast<CGenericProject*>(ui->tbMain->widget(index));

    if (!measObj)
    {
        if (!mp_dummyProject)
            mp_dummyProject = new CGenericProject(mp_serialThread, this);

        measObj = mp_dummyProject;
    }

    return measObj;
}

void MainWindow::setMachineState(EMachineState_t state)
{
    static EMachineState_t oldState = EMachineState_t::eDisconnected;
    m_machineState = state;

    static QIcon iconConnected;
    iconConnected.addFile(QStringLiteral(":/24/sign-ban-lighting.png"), QSize(), QIcon::Normal, QIcon::Off);

    static QIcon iconDisconnected;
    iconDisconnected.addFile(QStringLiteral(":/24/lightning.png"), QSize(), QIcon::Normal, QIcon::Off);

    QString port = CSettingsManager::instance()->paramValue(XML_FIELD_PORT);

    switch (state)
    {
        case EMachineState_t::eDisconnected:
        {
            ui->action_Connect->setEnabled(true);
            ui->action_Connect->setIcon(iconDisconnected);
            ui->action_Connect->setToolTip(QString("Connect to %1").arg(port));
            ui->action_Start_measure->setEnabled(false);
            ui->action_Pause_measure->setEnabled(false);
            ui->action_Cancel_measure->setEnabled(false);
            ui->statusBar->showMessage(QString("Disconnected from %1").arg(port), 5000);
            break;
        }

        case EMachineState_t::eConnected:
        {
            ui->action_Connect->setEnabled(true);
            ui->action_Connect->setIcon(iconConnected);
            ui->action_Connect->setToolTip(QString("Disconnect from %1").arg(port));
            ui->action_Start_measure->setEnabled(true);
            ui->action_Pause_measure->setEnabled(false);
            ui->action_Cancel_measure->setEnabled(false);
            break;
        }

        case EMachineState_t::eConnecting:
        {
            ui->action_Connect->setEnabled(false);
            ui->action_Connect->setIcon(iconDisconnected);
            ui->action_Connect->setToolTip(QString("Connecting to %1...").arg(port));
            ui->action_Start_measure->setEnabled(false);
            ui->action_Pause_measure->setEnabled(false);
            ui->action_Cancel_measure->setEnabled(false);
            ui->statusBar->showMessage(QString("Connecting to %1...").arg(port), 5000);
            break;
        }

        case EMachineState_t::eMeasuring:
        {
            ui->action_Connect->setEnabled(true);
            ui->action_Connect->setIcon(iconConnected);
            ui->action_Connect->setToolTip(QString("Disconnect from %1").arg(port));
            ui->action_Start_measure->setEnabled(false);
            ui->action_Pause_measure->setEnabled(true);
            ui->action_Cancel_measure->setEnabled(true);
            break;
        }

        default:
        {
            qWarning() << "Unknown machine state with code" << (int)state;
            m_machineState = oldState;
        }
    }

    if (m_machineState != oldState)
        oldState = m_machineState;
}

EMachineState_t& MainWindow::machineState()
{
    return m_machineState;
}

void MainWindow::on_action_Settings_triggered()
{
    CSettingsDialog settingsDial;
    settingsDial.exec();

    QString port = CSettingsManager::instance()->paramValue(XML_FIELD_PORT);
    mp_serialThread->updateSerialPort(port);
    ui->action_Connect->setToolTip(QString("Connect to %1").arg(port));
}

void MainWindow::on_action_New_triggered()
{
    EMeasures_t* newMeasure = new EMeasures_t;
    CNewProjectDialog newDial(newMeasure, this);
    CGenericProject* measIntstance = NULL;

    if (newDial.exec())
    {
        qDebug() << "New measure enum:" << (int)*newMeasure;

        switch (*newMeasure)
        {
            case EMeasures_t::eEIS:
            {
                measIntstance = new CEisProject(mp_serialThread);
                ui->tbMain->addTab(measIntstance, "Untitled* (EIS)");
                break;
            }

            case EMeasures_t::eCV:
            {
                measIntstance = new CCvProject(mp_serialThread);
                ui->tbMain->addTab(measIntstance, "Untitled* (CV)");
                break;
            }

            case EMeasures_t::eCA:
            {
                measIntstance = new CCaProject(mp_serialThread);
                ui->tbMain->addTab(measIntstance, "Untitled* (CA)");
                break;
            }

            case EMeasures_t::eDPV:
            {
                measIntstance = new CDpvProject(mp_serialThread);
                ui->tbMain->addTab(measIntstance, "Untitled* (DPV)");
                break;
            }

            default:
            {
                qWarning() << "Choosen unknown measure method, forgot to add?";
            }
        }
    }

    connect(measIntstance, SIGNAL(measureStarted()),
            this, SLOT(at_measureStarted()));
    connect(measIntstance, SIGNAL(measureFinished()),
            this, SLOT(at_measureFinished()));

    ui->tbMain->setCurrentIndex(ui->tbMain->indexOf(measIntstance));

    delete newMeasure;
}

void MainWindow::on_tbMain_tabCloseRequested(int index)
{
    qDebug() << "Close request " << index;

    disconnect(currentMeasObject(index), SIGNAL(measureStarted()),
            this, SLOT(at_measureStarted()));
    disconnect(currentMeasObject(index), SIGNAL(measureFinished()),
            this, SLOT(at_measureFinished()));

    delete  ui->tbMain->widget(index);
    //ui->tbMain->removeTab(index);
}

void MainWindow::on_tbMain_currentChanged(int index)
{
    //qDebug() << "current:" << ui->tbMain->widget(index)->metaObject()->className();
    qDebug() << "Current index changed" << index;

    checkCurrentTab(index);
}

void MainWindow::on_tbMain_objectNameChanged(const QString &objectName)
{
    qDebug() << "Object name changed" << objectName;
}

void MainWindow::on_action_Connect_triggered()
{
    //qDebug() << "Current object is of type" << (int)currentMeasObject()->measureType();

    if (EMachineState_t::eDisconnected == machineState())
    {
        mp_serialThread->start();
    }
    else
    {
        emit closeSerialThread();
        setMachineState(EMachineState_t::eDisconnected);

    }
}

void MainWindow::at_received_getFirmwareID(const MeasureUtility::union32_t& id)
{
    setMachineState(EMachineState_t::eConnected);
    ui->statusBar->showMessage(QString("Embedded system firmware version: %1.%2.%3.%4")
                              .arg(id.id8[3]).arg(id.id8[2]).arg(id.id8[1]).arg(id.id8[0]), 5000);
}

void MainWindow::at_measureStarted()
{
    qDebug() << "measure started!";
    setMachineState(EMachineState_t::eMeasuring);
    // ustawic aktywnego taba do pomiarow albo zablokowac funkcje zmieniajaca connecty
}

void MainWindow::at_measureFinished()
{
    qDebug() << "measure finished!";
    setMachineState(EMachineState_t::eConnected);
    // Odblokowac zmienianie connectow od tabow
}

void MainWindow::at_mp_SerialThread_rxTimeout(const int& command)
{
    setMachineState(EMachineState_t::eDisconnected);

    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText(QString("Communication with %1 failed")
                   .arg(CSettingsManager::instance()->paramValue(XML_FIELD_PORT)));
    msgBox.setInformativeText(QString("No communication with embedded system! Command %1 without answer")
                              .arg(command));
    msgBox.exec();
}

void MainWindow::at_mp_SerialThread_openPort(const int& val)
{
    if (val)
    {
        setMachineState(EMachineState_t::eDisconnected);

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QString("Port %1 is bussy").arg(CSettingsManager::instance()->paramValue(XML_FIELD_PORT)));
        msgBox.setInformativeText(QString("Cannot open specified port."));\
        msgBox.exec();
    }
    else
    {
        setMachineState(EMachineState_t::eConnecting);
        emit send_getFirmwareID();
    }
}

void MainWindow::on_action_Start_measure_triggered()
{
    if (machineState() == EMachineState_t::eConnected)
    {
        if((int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
        {
            currentMeasObject(ui->tbMain->currentIndex())->takeMeasure();
        }
    }
}

void MainWindow::on_action_Zoom_out_triggered()
{
    if((int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
    {
        currentMeasObject(ui->tbMain->currentIndex())->zoomOut();
    }
}

void MainWindow::on_action_Zoom_in_triggered()
{
    if((int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
    {
        currentMeasObject(ui->tbMain->currentIndex())->zoomIn();
    }
}

void MainWindow::on_action_Zoom_to_screen_triggered()
{
    if((int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
    {
        currentMeasObject(ui->tbMain->currentIndex())->zoomToPlot();
    }
}

void MainWindow::on_action_Export_CSV_triggered()
{
    if(!(int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
    {
        qCritical() << "No measure project opened. No CSV exported!";
        return;
    }

    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(QDir::currentPath());
    dialog.setWindowTitle("Export to CSV as...");
    dialog.setNameFilter(tr("CSV Files (*.csv)"));
    dialog.setViewMode(QFileDialog::Detail);

    QStringList fileNames;
    if(dialog.exec())
        fileNames = dialog.selectedFiles();

    if (fileNames.length() > 1)
    {
        QMessageBox msgBox;
        msgBox.setText("File save error!");
        msgBox.setInformativeText("Cannot select multiple files!");
        msgBox.exec();
        return;
    }
    else if (0 == fileNames.length())
        return;

    if (!fileNames[0].endsWith(".csv"))
        fileNames[0].append(".csv");

    saveCsvFile(fileNames[0]);
}

void MainWindow::saveCsvFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        QMessageBox::warning(this, tr("Impedance Manager"),
                             tr("Cannot save file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return;
    }

    if((int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
    {
        if (!currentMeasObject(ui->tbMain->currentIndex())->saveToCsv(&file))
        {
            ui->statusBar->showMessage(QString("File %1 exported to CSV successfuly").arg(fileName), 5000);
            file.close();
            return;
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText("File open error!");
            msgBox.setInformativeText("Saving file failed!");
            msgBox.exec();
            file.close();
            return;
        }
    }
    else
    {
        qCritical() << "No measure project opened. No CSV exported!";
    }
}

const QString MainWindow::getNameForSave()
{
    if(!(int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
    {
        qCritical() << "No measure project opened. No project saved";
        return NULL;
    }

    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDirectory(QDir::currentPath());
    dialog.setWindowTitle("Save project as...");
    dialog.setNameFilter(tr("Impedance Manager Project files (*.imp)"));
    dialog.setViewMode(QFileDialog::Detail);

    QStringList fileNames;
    if(dialog.exec())
        fileNames = dialog.selectedFiles();

    if (fileNames.length() > 1)
    {
        QMessageBox msgBox;
        msgBox.setText("File save error!");
        msgBox.setInformativeText("Cannot select multiple files!");
        msgBox.exec();
        return NULL;
    }
    else if (0 == fileNames.length())
        return NULL;

    if (!fileNames[0].endsWith(".imp"))
        fileNames[0].append(".imp");

    return fileNames[0];
}

const QString MainWindow::getNameForOpen()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Profile File"),
                                                    QDir::currentPath(),
                                                    tr("IMP Files (*.imp)"));
    return fileName;
}

int MainWindow::saveProject(const QString& fileName)
{
    QFile file(fileName);
    int index = ui->tbMain->currentIndex();

    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        QMessageBox::warning(this, tr("Impedance Manager"),
                             tr("Cannot save file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return -1;
    }

    if((int)currentMeasObject(index)->measureType())
    {
        if (!currentMeasObject(index)->saveProjectAs(file))
        {
            ui->statusBar->showMessage(QString("Project %1 saved successfuly").arg(fileName), 5000);
            file.close();

            // update tab
            currentMeasObject(index)->setWorkingFile(fileName);
            QFileInfo fi = fileName;
            ui->tbMain->setTabText(index, fi.baseName());

            // update bar title
            setWindowTitle(APPNAME + getAppVersion() + " - " + fileName);
            return 0;
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText("File open error!");
            msgBox.setInformativeText("Saving file failed!");
            msgBox.exec();
            file.close();
            return -2;
        }
    }
    else
    {
        qCritical() << "No measure project opened. No project saved!";
    }

    return -3;
}

void MainWindow::on_action_Points_labels_triggered()
{
    if(!(int)currentMeasObject(ui->tbMain->currentIndex())->measureType())
    {
        qCritical() << "Cannot toggle labels for non-opened measure profile";
        return;
    }

    currentMeasObject(ui->tbMain->currentIndex())->toggleLabels();
}

void MainWindow::on_action_About_triggered()
{
    QString filePatch = QApplication::applicationDirPath() + "/changelog.txt";
    QFile f(filePatch);
    if (!f.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("File open error!");
        msgBox.setInformativeText("changelog.txt not found");
        msgBox.exec();

        return;
    }

    QTextStream in(&f);
    CAboutDialog aboutDial(APPNAME + getAppVersion(), in.readAll());
    aboutDial.exec();
}

void MainWindow::checkCurrentTab(int index)
{
    bool enableVar = false;

    // check if any tab is opened, otherwise no tabs left to select
    if (index >= 0)
    {
        // disconnect all
        for (int i = 0; i < ui->tbMain->count(); i++)
            currentMeasObject(i)->changeConnections(false);

        // and connect the current one
        currentMeasObject(index)->changeConnections(true);

        // for action buttons
        enableVar = true;

        // get project filename and update title bar
        setWindowTitle(APPNAME + getAppVersion() + " - " +
                       currentMeasObject(index)->workingFile());
    }

    ui->action_Save->setEnabled(enableVar);
    ui->action_Save_as->setEnabled(enableVar);
}

void MainWindow::on_action_Save_triggered()
{
    qDebug() << "Save pressed.";
    int index = ui->tbMain->currentIndex();

    // check if a measure tab is opened
    if((int)currentMeasObject(index)->measureType())
    {
        QString workingFile = currentMeasObject(index)->workingFile();

        // Now check either we are working on untitled document, or saved already
        if (NULL == workingFile) // untitled, save as
        {
            workingFile = getNameForSave();

            if (NULL == workingFile)
            {
                qWarning() << "Empty filename, project cannot be saved!";
                return;
            }
        }

        saveProject(workingFile);
    }
    else
    {
        qDebug() << "Cannot save, no measure type selected";
    }
}

void MainWindow::on_action_Save_as_triggered()
{
    qDebug() << "Save as... pressed.";
    int index = ui->tbMain->currentIndex();

    // check if a measure tab is opened
    if((int)currentMeasObject(index)->measureType())
    {
        // do save as...
        QString workingFile = getNameForSave();

        if (NULL == workingFile)
        {
            qWarning() << "Empty filename, project cannot be saved!";
            return;
        }

        saveProject(workingFile);
    }
    else
    {
        qDebug() << "Cannot save, no measure type selected";
    }
}

void MainWindow::openProject(const QString& fileName)
{
    // get measure type from the file
    QFile file(fileName);

    if(!file.exists())
    {
        qWarning() << "File " + fileName + " doesnt exist";
        return;
    }

    if (fileName.isEmpty())
    {
        qWarning() << "File " + fileName + " is empty";
        return;
    }

    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        qWarning() << "Cannot open " + fileName;
        return;
    }

    QXmlStreamReader xr(&file);
    while (!xr.atEnd())
    {
        if (xr.readNextStartElement())
        {
            if (xr.name() != "project")
            {
                if (xr.name().toString() == "measType")
                {
                    EMeasures_t* newMeasure = new EMeasures_t;
                    CGenericProject* measInstance = NULL;
                    *newMeasure = (EMeasures_t)xr.readElementText().toInt();

                    switch (*newMeasure)
                    {
                        case EMeasures_t::eEIS:
                        {
                            measInstance = new CEisProject(mp_serialThread);
                            break;
                        }

                        case EMeasures_t::eCV:
                        {
                            measInstance = new CCvProject(mp_serialThread);
                            break;
                        }

                        case EMeasures_t::eCA:
                        {
                            measInstance = new CCaProject(mp_serialThread);
                            break;
                        }

                        case EMeasures_t::eDPV:
                        {
                            measInstance = new CDpvProject(mp_serialThread);
                            break;
                        }

                        default:
                        {
                            qWarning() << "Choosen unknown measure method, forgot to add?";
                        }
                    }

                    delete newMeasure;
                    file.close();

                    // update tab
                    QFileInfo fi = fileName;
                    ui->tbMain->addTab(measInstance, fi.baseName());
                    measInstance->setWorkingFile(fileName);

                    // read the fields
                    measInstance->openProject(file);

                    // testing:
                    //measInstance->takeMeasure();
                    return;
                }
            }
        }
    }

    file.close();
    return;
}

void MainWindow::on_action_Open_triggered()
{
    QString filename = getNameForOpen();
    qDebug() << "Opening file " + filename;

    if (filename != NULL)
        openProject(filename);
}
