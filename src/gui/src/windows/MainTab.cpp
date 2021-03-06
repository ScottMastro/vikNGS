#include "MainWindow.h"
#include "PlotWindow.h"
#include "ui_mainwindow.h"
#include "../log/TypeConverter.h"

void MainWindow::on_main_vcfDirBtn_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), lastDirectory,
                                                    tr("VCF File (*.vcf);;All files (*.*)"));

    if(!fileName.isNull()){
        ui->main_vcfDirTxt->setText(fileName);
        QFileInfo fi(fileName);
        lastDirectory = fi.absolutePath();
    }
}

void MainWindow::on_main_sampleDirBtn_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), lastDirectory,
                                                    tr("Text File (*.txt);;All files (*.*)"));
    if(!fileName.isNull()){
        ui->main_sampleDirTxt->setText(fileName);
        QFileInfo fi(fileName);
        lastDirectory = fi.absolutePath();
    }
}

void MainWindow::on_main_bedDirBtn_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), lastDirectory,
                                                    tr("BED File (*.bed);;Text File (*.txt);;All files (*.*)"));
    if(!fileName.isNull()){
        ui->main_bedDirTxt->setText(fileName);
        QFileInfo fi(fileName);
        lastDirectory = fi.absolutePath();
    }
}

void MainWindow::on_main_outDirBtn_pressed()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Open Directory"), lastDirectory);

    if(!dirName.isNull()){
        lastDirectory = dirName;
        ui->main_outDirTxt->setText(dirName);
    }
}

void MainWindow::on_main_testRareCastBtn_toggled(bool checked){
    if(checked)
        ui->main_testBootChk->setChecked(true);
}

void MainWindow::on_main_testRareSkatBtn_toggled(bool checked){
    if(checked)
        ui->main_testBootChk->setChecked(true);
}

void MainWindow::on_main_testBootChk_stateChanged(int arg1)
{
    ui->main_testBootTxt->setEnabled(ui->main_testBootChk->isChecked());
    ui->main_testStopChk->setEnabled(ui->main_testBootChk->isChecked());
    ui->main_testBootLbl->setEnabled(ui->main_testBootChk->isChecked());

}
void MainWindow::on_main_testBootChk_toggled(bool checked)
{
    if(ui->main_testRareCastBtn->isChecked() ||
            ui->main_testRareSkatBtn->isChecked())
        ui->main_testBootChk->setCheckState(Qt::CheckState::Checked);
}

void MainWindow::on_main_vcfWholeFileChk_toggled(bool checked){

    ui->main_vcfChrLbl->setEnabled(!checked);
    ui->main_vcfChromFilterTxt->setEnabled(!checked);
    ui->main_vcfFromPosLbl->setEnabled(!checked);
    ui->main_vcfToPosLbl->setEnabled(!checked);
    ui->main_vcfFromPosTxt->setEnabled(!checked);
    ui->main_vcfToPosTxt->setEnabled(!checked);

}

void MainWindow::on_main_runBtn_clicked() {

    if(!ui->main_runBtn->isEnabled())
        return;

    greyOutput();
    disableRun();

    try{

        Request req = createRequest();

        jobThread = new QThread;
        AsyncJob* job = new AsyncJob();
        job->setRequest(req);
        job->moveToThread(jobThread);
        connect(jobThread, SIGNAL(started()), job, SLOT(runVikngs()));
        connect(job, SIGNAL(complete()), jobThread, SLOT(quit()));
        connect(job, SIGNAL(complete()), job, SLOT(deleteLater()));
        connect(job, SIGNAL(jobFinished(Data, bool)), this, SLOT(jobFinished(Data, bool)));

        jobThread->start();

    }catch(...){
        enableRun();
    }
}

Request MainWindow::createRequest(){

    Request req = getDefaultRequest();

    req.setKeepFiltered(ui->main_explainFilterChk->isChecked());
    req.setRetainGenotypes(ui->main_retainGtChk->isChecked());
    req.setMakePlot(ui->main_plotChk->isChecked());

    QVector<std::string> commands;
    commands.push_back("vikNGS");

    printInfo("Validating parameters...");

    std::string vcfDir = ui->main_vcfDirTxt->text().toStdString();
    printInfo("VCF file: " + vcfDir);
    //commands.push_back("--vcf " + vcfDir);
    commands.push_back("--vcf " + vcfDir);
    std::string sampleDir = ui->main_sampleDirTxt->text().toStdString();
    printInfo("Sample info file: " + sampleDir);
    //commands.push_back("--sample " + sampleDir);
    commands.push_back("--sample " + sampleDir);

    req.setInputFiles(vcfDir, sampleDir);

    std::string outDir = ui->main_outDirTxt->text().toStdString();
    if(outDir.size() > 0){
        printInfo("Output directory: " + outDir);
        commands.push_back("-o " + sampleDir);
        req.setOutputDir(outDir);
    }

    std::string mafCutOff = ui->main_vcfMafTxt->text().toStdString();
    double maf = toDouble("Minor allele frequency threshold", mafCutOff);
    printInfo("Output directory: " + sampleDir);
    mafCutOff = toString(maf);
    commands.push_back("-m " + mafCutOff);
    printInfo("Minor allele frequency threshold: " + mafCutOff);
    req.setMafCutOff(maf);

    std::string highLowCutOff = ui->main_sampleDepthTxt->text().toStdString();
    int depth = toInt("Read depth threshold", highLowCutOff);
    highLowCutOff = toString(depth);
    commands.push_back("-d " + highLowCutOff);
    printInfo("Read depth threshold: " + highLowCutOff);
    req.setHighLowCutOff(depth);

    std::string missingThreshold = ui->main_vcfMissingTxt->text().toStdString();
    double missing = toDouble("Missing data threshold", missingThreshold);
    missingThreshold = toString(missing);
    commands.push_back("-x " + missingThreshold);
    printInfo("Missing data threshold: " + missingThreshold);
    req.setMissingThreshold(missing);

    if(!ui->main_vcfWholeFileChk->isChecked()){

        std::string chrFilter = ui->main_vcfChromFilterTxt->text().toStdString();
        if(chrFilter.size() > 0){
            commands.push_back("--chr " + chrFilter);
            printInfo("Analyzing variants on chromosome " + chrFilter);
            req.setChromosomeFilter(chrFilter);
        }

        std::string fromPos = ui->main_vcfFromPosTxt->text().toStdString();
        if(fromPos.size() > 0){
            int from = toInt("Filter from position", fromPos);
            fromPos = toString(from);
            commands.push_back("--from " + fromPos);
            printInfo("Analyzing variants with POS greater than " + fromPos);
            req.setMinPos(from);
        }

        std::string toPos = ui->main_vcfToPosTxt->text().toStdString();
        if(toPos.size() > 0){
            int to = toInt("Filter to position", toPos);
            toPos = toString(to);
            commands.push_back("--to " + toPos);
            printInfo("Analyzing variants with POS less than " + toPos);
            req.setMaxPos(to);
        }
    }

    bool mustPass = ui->main_vcfPassChk->isChecked();
    if(!mustPass){
        commands.push_back("-a");
        printInfo("Retain variants which do not PASS");
    }
    else
        printInfo("Remove variants which do not PASS");
    req.setMustPASS(mustPass);

    int n = 0;
    if(ui->main_testBootChk->isChecked()){

        std::string nboot = ui->main_testBootTxt->text().toStdString();
        bool stopEarly = ui->main_testStopChk->isChecked();

        n = toInt("Bootstrap iterations", nboot);

        if(n > 1){
            req.setBootstrap(n);
            nboot = toString(n);
            req.setStopEarly(stopEarly);
            if(stopEarly){
                printInfo("Using " + nboot + " bootstrap iterations and early stopping");
                commands.push_back("-s");
            }
            else
                printInfo("Using " + nboot + " bootstrap iterations");

            commands.push_back("-n " + nboot);
        }
    }

    if(ui->main_testCommonBtn->isChecked()){

        if(ui->main_vcfGT->isChecked())
            req.addTest(TestSettings(GenotypeSource::VCF_CALL, Statistic::COMMON, Variance::REGULAR));
        if(ui->main_rvsChk->isChecked())
            req.addTest(TestSettings(GenotypeSource::EXPECTED, Statistic::COMMON, Variance::RVS));
        if(ui->main_gtChk->isChecked())
            req.addTest(TestSettings(GenotypeSource::CALL, Statistic::COMMON, Variance::REGULAR));

        printInfo("Preparing to run common variant association...");
        commands.push_back("-c");
    }

    if(ui->main_testRareCastBtn->isChecked()){

        if(ui->main_vcfGT->isChecked())
            req.addTest(TestSettings(GenotypeSource::VCF_CALL, Statistic::CAST, Variance::REGULAR));
        if(ui->main_rvsChk->isChecked())
            req.addTest(TestSettings(GenotypeSource::EXPECTED, Statistic::CAST, Variance::RVS));
        if(ui->main_gtChk->isChecked())
            req.addTest(TestSettings(GenotypeSource::CALL, Statistic::CAST, Variance::REGULAR));

        printInfo("Preparing to run rare variant association (CAST p-values)...");
        commands.push_back("-r cast");
    }

    if(ui->main_testRareSkatBtn->isChecked()){

        if(ui->main_vcfGT->isChecked())
            req.addTest(TestSettings(GenotypeSource::VCF_CALL, Statistic::SKAT, Variance::REGULAR));
        if(ui->main_rvsChk->isChecked())
            req.addTest(TestSettings(GenotypeSource::EXPECTED, Statistic::SKAT, Variance::RVS));
        if(ui->main_gtChk->isChecked())
            req.addTest(TestSettings(GenotypeSource::CALL, Statistic::SKAT, Variance::REGULAR));

        printInfo("Preparing to run rare variant association (SKAT p-values)...");
        commands.push_back("-r skat");
    }

    std::string bedDir = ui->main_bedDirTxt->text().toStdString();
    bool collapseGene = ui->main_bedCollapseGeneBtn->isChecked();
    bool collapseExon = ui->main_bedCollapseExonBtn->isChecked();
    //bool collapseK = ui->main_bedCollapseKBtn->isChecked();

    if(ui->main_testRareSkatBtn->isChecked() || ui->main_testRareCastBtn->isChecked()){
        std::string everyk = ui->main_bedCollapseKTxt->text().toStdString();
        int k = toInt("Collapse k value", everyk);
        everyk = toString(k);
        commands.push_back("-k " + everyk);
        printInfo("Collapse to " + everyk + " variants");
        req.setCollapse(k);
    }

    if(bedDir.size() > 0){

        req.setCollapseFile(bedDir);
        printInfo("BED file: " + bedDir);
        //commands.push_back("-b " + bedDir);
        commands.push_back("-b [...]");

        if(collapseGene){
            printInfo("Collapse variants along genes");
            commands.push_back("--gene");
            req.setCollapseGene();
        }
        else if(collapseExon){
            printInfo("Collapse variants along exons");
            commands.push_back("--exon");
            req.setCollapseExon();
        }
    }

    std::string batchSize = ui->main_batchSizeTxt->text().toStdString();
    int batch = toInt("Batch size", batchSize);
    batchSize = toString(batch);
    commands.push_back("-h " + batchSize);
    printInfo("Batch size: " + batchSize);
    req.setBatchSize(batch);

    std::string nthreads = ui->main_threadsTxt->text().toStdString();
    int threads = toInt("Number of threads", nthreads);
    nthreads = toString(threads);
    if(threads != 1){
        printInfo("Using " + nthreads + " threads");
        commands.push_back("-t " + nthreads);
    }
    req.setNumberThreads(threads);

    QString command = "";
    for (int i = 0; i<commands.size(); i++)
        command.append(QString::fromStdString(commands[i]) + " ");

    printOutput("\n---------------------", green);
    printOutput("Command Line :", green);
    printOutput(command, green);
    printOutput("---------------------\n", green);

    return req;
}

