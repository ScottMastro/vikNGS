#include "MainWindow.h"
#include "SimPlotWindow.h"

#include "ui_mainwindow.h"

const QString sep = ":";

void MainWindow::simulationTabInit(){

    addGroup("100", false, "32", "8", "0.01");
    addGroup("100:300", true, "6", "2", "0.01");
}

std::vector<SimulationRequestGroup> MainWindow::constructGroups(int ntest){

    std::vector<SimulationRequestGroup> groups;

    for(int i = 0; i< ui->sim_groupTbl->rowCount(); i++){

        SimulationRequestGroup g;

        QString size = ui->sim_groupTbl->item(i, 0)->text();

        if(size.contains(sep)){

            bool ok1 = false;
            bool ok2 = false;

            int nMin = size.split(sep).at(0).toInt(&ok1);
            int nMax = size.split(sep).at(1).toInt(&ok2);
            if(!ok1 || !ok2 || nMin <= 0 || nMax <= 0)
                throwError(ERROR_SOURCE, "Invalid group settings. Ensure number of individuals is an integer greater than 0. If you wish to use a range please separate two integers by a colon \":\"");


            if(nMax < nMin){
                int temp = nMax;
                nMax = nMin;
                nMin = temp;
            }

            g.n=nMin;
            g.n_increment = (nMax - nMin)/std::max(1, ntest-1);

        }
        else{
            g.n = size.toInt();
            g.n_increment=0;
        }

        QString caseControl = ((QComboBox*)ui->sim_groupTbl->indexWidget(ui->sim_groupTbl->model()->index(i, 1)))->currentText();
        QString meanDepth = ui->sim_groupTbl->item(i, 2)->text();
        QString sdDepth = ui->sim_groupTbl->item(i, 3)->text();
        QString errorRate = ui->sim_groupTbl->item(i, 4)->text();

        g.index = i;

        bool ok = false;

        g.meanDepth = meanDepth.toDouble(&ok);
        if(!ok || g.meanDepth <= 0)
            throwError(ERROR_SOURCE, "Invalid group settings. Expected mean read depth to be a numeric value greater than 0.", std::to_string(g.meanDepth));


        ok=false;
        g.sdDepth = sdDepth.toDouble(&ok);
        if(!ok || g.sdDepth < 0)
            throwError(ERROR_SOURCE, "Invalid group settings. Expected read depth SD to be a numeric value greater than or equal to 0.", std::to_string(g.sdDepth));


        ok=false;
        g.errorRate = errorRate.toDouble(&ok);
        if(!ok || g.errorRate < 0 || g.errorRate > 1)
            throwError(ERROR_SOURCE, "Invalid group settings. Expected error rate to be a numeric value between 0 and 1", std::to_string(g.errorRate));


        g.isCase = (caseControl == "case");

        ok=false;
        int highLow = ui->sim_groupHighLowTxt->text().toInt(&ok);

        if(!ok || highLow <= 0)
            throwError(ERROR_SOURCE, "Expected high-low read depth cutoff to be an integer greater than 0", std::to_string(highLow));


       (g.meanDepth >= highLow) ? g.readDepth = Depth::HIGH : g.readDepth = Depth::LOW;

        g.family=Family::BINOMIAL;
        groups.push_back(g);
    }

    return groups;
}

SimulationRequest MainWindow::constructRequest(std::vector<SimulationRequestGroup> groups){

    SimulationRequest request;
    request.family = Family::BINOMIAL;
    request.groups = groups;

    request.testStatistic = Statistic::COMMON;
    request.useBootstrap = false;

    if(ui->sim_testRareSkatBtn->isChecked()){
        request.testStatistic = Statistic::SKAT;
        request.collapse = ui->sim_collapseTxt->text().toInt();
        request.useBootstrap = true;

    }
    else if(ui->sim_testRareCastBtn->isChecked()){
        request.testStatistic = Statistic::CAST;
        request.collapse = ui->sim_collapseTxt->text().toInt();
        request.useBootstrap = true;
    }

    request.stopEarly = false;
    if (request.useBootstrap){
        request.nboot = ui->sim_testBootTxt->text().toInt();
        request.stopEarly = ui->sim_testStopChk->isChecked();
    }
    else
        request.nboot=0;

    request.nsnp = ui->sim_variantSizeTxt->text().toInt();
    request.oddsRatio = ui->sim_oddsRatioTxt->text().toDouble();
    request.mafMin = ui->sim_variantMafMinTxt->text().toDouble();
    request.mafMax = ui->sim_variantMafMaxTxt->text().toDouble();

    return request;
}

void MainWindow::simulationFinished(Data results, SimulationRequest req){
    enableRun();

    if(results.size() > 0){
        SimPlotWindow *plotter = new SimPlotWindow();
        QString title = "Plot " + QString::number(plotCount);
        plotCount++;
        plotter->initialize(results, req, title);
        printOutput("Displaying results in " + title.toLower(), green);
        plotter->show();
    }
}

void MainWindow::on_sim_stopBtn_clicked(){
    STOP_RUNNING_THREAD = true;
    while(!jobThread->isFinished()){
        jobThread->quit();
    }
    greyOutput();
    printInfo("Job stopped.");
    enableRun();
    STOP_RUNNING_THREAD=false;
}

void MainWindow::on_sim_runBtn_clicked(){
    if(!ui->sim_runBtn->isEnabled())
        return;

    greyOutput();
    disableRun();

    SimulationRequest request;
    bool ok=false;

    bool multitest = false;
    for(int i = 0; i< ui->sim_groupTbl->rowCount(); i++)
        if(ui->sim_groupTbl->item(i, 0)->text().contains(sep))
            multitest = true;

    try{
        int steps = 1;
        if(multitest){

            bool ok=false;
            steps = ui->sim_powerStepTxt->text().toInt(&ok);
            if(!ok || steps < 1)
                throwError(ERROR_SOURCE, "Invalid step value. Expected # steps to be an integer greater than 1.", ui->sim_powerStepTxt->text().toStdString());
        }

        std::vector<SimulationRequestGroup> groups = constructGroups(steps);
        request = constructRequest(groups);
        request.steps = steps;

        int nthreads = ui->sim_threadsTxt->text().toInt(&ok);
        if(!ok)
            throwError(ERROR_SOURCE, "Invalid number of threads. Expected an integer greater than or equal to 1.", ui->sim_threadsTxt->text().toStdString());

        request.nthreads = nthreads;

        //throws error if invalid
        request.validate();

        QThread* thread = new QThread;
        AsyncJob* runner = new AsyncJob();

        runner->setSimulationRequest(request);
        runner->moveToThread(thread);
        connect(thread, SIGNAL(started()), runner, SLOT(runSimulation()));
        connect(runner, SIGNAL(complete()), thread, SLOT(quit()));
        connect(runner, SIGNAL(complete()), runner, SLOT(deleteLater()));
        connect(runner, SIGNAL(simulationFinished(Data, SimulationRequest)),
                this, SLOT(simulationFinished(Data, SimulationRequest)));
        thread->start();
        jobThread = thread;
    }catch(...){
        ui->sim_runBtn->setEnabled(true);
        enableRun();
    }
}

void MainWindow::on_sim_groupRemoveBtn_clicked()
{
    QList<QPersistentModelIndex> indexes;

    foreach (const QModelIndex &i, ui->sim_groupTbl->selectionModel()->selectedIndexes())
        indexes << i;

    foreach (const QPersistentModelIndex &i, indexes)
        ui->sim_groupTbl->model()->removeRow(i.row());


    int index = ui->sim_groupTbl->rowCount();
    ui->sim_groupTbl->selectRow(index-1);

}

void MainWindow::simEnableRare(bool value){
    ui->sim_collapseTxt->setEnabled(value);
    ui->sim_collapseLbl->setEnabled(value);
    ui->sim_testBootLbl->setEnabled(value);
    ui->sim_testBootTxt->setEnabled(value);
    ui->sim_testStopChk->setEnabled(value);
}

void MainWindow::on_sim_testRareCastBtn_toggled(bool checked){
    simEnableRare(checked);
}

void MainWindow::on_sim_testRareSkatBtn_toggled(bool checked){
    simEnableRare(checked);
}

void MainWindow::addGroup(QString n, bool control, QString depth, QString sdDepth, QString errorRate){

    ui->sim_groupTbl->setSelectionBehavior(QAbstractItemView::SelectRows);

    int index = ui->sim_groupTbl->rowCount();

    ui->sim_groupTbl->insertRow(index);

    QTableWidgetItem* item;

    item = new QTableWidgetItem(n);
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
    ui->sim_groupTbl->setItem(index, 0, item);

    QComboBox *caseControlBox = new QComboBox();
    caseControlBox->addItem("case");
    caseControlBox->addItem("control");

    if(control)
        caseControlBox->setCurrentIndex(1);

    ui->sim_groupTbl->setIndexWidget(ui->sim_groupTbl->model()->index(index, 1), caseControlBox);

    item = new QTableWidgetItem(depth);
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
    ui->sim_groupTbl->setItem(index, 2, item);

    item = new QTableWidgetItem(sdDepth);
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
    ui->sim_groupTbl->setItem(index, 3, item);

    item = new QTableWidgetItem(errorRate);
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
    ui->sim_groupTbl->setItem(index, 4, item);

    ui->sim_groupTbl->selectRow(index);
    ui->sim_groupTbl->update();
}

void MainWindow::on_sim_groupAddBtn_clicked(){
    addGroup("100", true, "10", "2", "0.01");
}

