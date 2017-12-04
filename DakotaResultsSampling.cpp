// Written: fmckenna

#include "DakotaResultsSampling.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>

#include <QTabWidget>
#include <QTextEdit>
#include <MyTableWidget.h>
#include <QDebug>
#include <QHBoxLayout>
#include <QColor>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include <QMessageBox>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QVXYModelMapper>
using namespace QtCharts;


DakotaResultsSampling::DakotaResultsSampling(QWidget *parent)
    : DakotaResults(parent)
{
    // title & add button
    tabWidget = new QTabWidget(this);
    layout->addWidget(tabWidget,1);
    mLeft = true;
}

DakotaResultsSampling::~DakotaResultsSampling()
{

}


void DakotaResultsSampling::clear(void)
{
    QWidget *res=tabWidget->widget(0);
    QWidget *gen=tabWidget->widget(0);
    QWidget *dat=tabWidget->widget(0);

    tabWidget->clear();
    delete dat;
    delete gen;
    delete res;
}



void
DakotaResultsSampling::outputToJSON(QJsonObject &jsonObject)
{
    jsonObject["resultType"]=QString(tr("DakotaResultsSampling"));

    //
    // add summary data
    //

    QJsonArray resultsData;
    int numEDP = theNames.count();
    for (int i=0; i<numEDP; i++) {
        QJsonObject edpData;
        edpData["name"]=theNames.at(i);
        edpData["mean"]=theMeans.at(i);
        edpData["stdDev"]=theStdDevs.at(i);
        resultsData.append(edpData);
    }
    jsonObject["summary"]=resultsData;


    // add general data
    jsonObject["general"]=dakotaText->toPlainText();

    //
    // add spreadsheet data
    //

    QJsonObject spreadsheetData;

    int numCol = spreadsheet->columnCount();
    int numRow = spreadsheet->rowCount();

    spreadsheetData["numRow"]=numRow;
    spreadsheetData["numCol"]=numCol;

    QJsonArray headingsArray;
    for (int i = 0; i <theHeadings.size(); ++i) {
        headingsArray.append(QJsonValue(theHeadings.at(i)));
    }

    spreadsheetData["headings"]=headingsArray;

    QJsonArray dataArray;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (int row = 0; row < numRow; ++row) {
        for (int column = 0; column < numCol; ++column) {
            QTableWidgetItem *theItem = spreadsheet->item(row,column);
            QString textData = theItem->text();
            dataArray.append(textData.toDouble());
        }
    }
    QApplication::restoreOverrideCursor();
    spreadsheetData["data"]=dataArray;

    jsonObject["spreadsheet"] = spreadsheetData;
}


void
DakotaResultsSampling::inputFromJSON(QJsonObject &jsonObject)
{
    this->clear();

    //
    // create a summary widget in which place basic output (name, mean, stdDev)
    //

    QWidget *summary = new QWidget();
    QVBoxLayout *summaryLayout = new QVBoxLayout();
    summary->setLayout(summaryLayout);

    QJsonArray edpArray = jsonObject["summary"].toArray();
    foreach (const QJsonValue &edpValue, edpArray) {
        QString name;
        double mean, stdDev;
        QJsonObject edpObject = edpValue.toObject();
        QJsonValue theNameValue = edpObject["name"];
        name = theNameValue.toString();

        QJsonValue theMeanValue = edpObject["mean"];
        mean = theMeanValue.toDouble();

        QJsonValue theStdDevValue = edpObject["stdDev"];
        stdDev = theStdDevValue.toDouble();

        QWidget *theWidget = this->createResultEDPWidget(name, mean, stdDev);
        summaryLayout->addWidget(theWidget);
    }
    summaryLayout->addStretch();

    //
    // into a QTextEdit place more detailed Dakota text
    //

    dakotaText = new QTextEdit();
    dakotaText->setReadOnly(true); // make it so user cannot edit the contents
    QJsonValue theValue = jsonObject["general"];
    dakotaText->setText(theValue.toString());

    //
    // into a spreadsheet place all the data returned
    //

    spreadsheet = new MyTableWidget();
    QJsonObject spreadsheetData = jsonObject["spreadsheet"].toObject();
    int numRow = spreadsheetData["numRow"].toInt();
    int numCol = spreadsheetData["numCol"].toInt();
    spreadsheet->setColumnCount(numCol);
    spreadsheet->setRowCount(numRow);

    QJsonArray headingData= spreadsheetData["headings"].toArray();
    for (int i=0; i<numCol; i++) {
        theHeadings << headingData.at(i).toString();
    }

    spreadsheet->setHorizontalHeaderLabels(theHeadings);

    QJsonArray dataData= spreadsheetData["data"].toArray();
    int dataCount =0;
    for (int row =0; row<numRow; row++) {
        for (int col=0; col<numCol; col++) {
            QModelIndex index = spreadsheet->model()->index(row, col);
            spreadsheet->model()->setData(index, dataData.at(dataCount).toDouble());
            dataCount++;
        }
    }
    spreadsheet->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(spreadsheet,SIGNAL(cellPressed(int,int)),this,SLOT(onSpreadsheetCellClicked(int,int)));

    //
    // create a chart, setting data points from first and last col of spreadsheet
    //

    chart = new QChart();
    chart->setAnimationOptions(QChart::AllAnimations);
    QScatterSeries *series = new QScatterSeries;
    col1 = 0;
    col2 = numCol-1;
    mLeft = true;

    this->onSpreadsheetCellClicked(0,numCol-1);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->chart()->legend()->hide();

    //
    // create a widget into which we place the chart and the spreadsheet
    //

    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->addWidget(chartView, 1);
    layout->addWidget(spreadsheet, 1);

    //
    // add 3 Widgets to TabWidget
    //

    tabWidget->addTab(summary,tr("Summmary"));
    tabWidget->addTab(dakotaText, tr("General"));
    tabWidget->addTab(widget, tr("Data Values"));

    tabWidget->adjustSize();
}



int DakotaResultsSampling::processResults(QString &filenameResults, QString &filenameTab) {

    //
    // get a Qwidget ready to place summary data, the EDP name, mean, stdDev into
    //

    QWidget *summary = new QWidget();
    QVBoxLayout *summaryLayout = new QVBoxLayout();
    summary->setLayout(summaryLayout);

    //
    // into a QTextEdit we will place contents of Dakota more detailed output
    //

    dakotaText = new QTextEdit();
    dakotaText->setReadOnly(true); // make it so user cannot edit the contents

    dakotaText->setText("\n");

    //
    // open Dakota output file
    //

    std::ifstream fileResults(filenameResults.toStdString().c_str());
    if (!fileResults.is_open()) {
        qDebug() << "Could not open file: " << filenameResults;
        return -1;
    }

    // now ignore every line until Kurtosis found

    const std::string needle = "Kurtosis";
    std::string haystack;

    while (std::getline(fileResults, haystack)) {
        if (haystack.find(needle) != std::string::npos) {
            break;
        }
    }

    //
    // now copy line and every subsequent line into text editor
    //  - also if still collecting sumary data, add each EDP's sumary info
    //

    dakotaText->append(haystack.c_str());

    bool isSummaryDone = false;

    while (std::getline(fileResults, haystack)) {
        dakotaText->append(haystack.c_str());
        if (isSummaryDone == false) {

            if ( strlen(haystack.c_str()) == 0) {
                isSummaryDone = true;
            } else {
                //
                // add sumary info
                //

                std::istringstream iss(haystack);
                std::string subs;

                iss >> subs;
                QString  nameString(QString::fromStdString(subs));

                iss >> subs;
                QString meanText(QString::fromStdString(subs));
                double mean = meanText.toDouble();

                iss >> subs;
                QString stdDevText(QString::fromStdString(subs));
                double stdDev = stdDevText.toDouble();

                QWidget *theWidget = this->createResultEDPWidget(nameString, mean, stdDev);
                summaryLayout->addWidget(theWidget);
            }
        }
    }
    summaryLayout->addStretch();

    // close input file
    fileResults.close();

    //
    // now into a QTableWidget copy the random variable and edp's of each black box run
    //

    spreadsheet = new MyTableWidget();

    // open file containing tab data
    std::ifstream tabResults(filenameTab.toStdString().c_str());
    if (!tabResults.is_open()) {
        qDebug() << "Could not open file";
        return -1;
    }

    //
    // read first line and set headings (ignoring second column for now)
    //

    std::string inputLine;
    std::getline(tabResults, inputLine);
    std::istringstream iss(inputLine);
    int colCount = 0;
    theHeadings << "Rum #";
    do
    {
        std::string subs;
        iss >> subs;
        if (colCount > 1) {
            if (subs != " ") {
                theHeadings << subs.c_str();
            }
        }
        colCount++;
    } while (iss);

    colCount = colCount-2;
    spreadsheet->setColumnCount(colCount);
    spreadsheet->setHorizontalHeaderLabels(theHeadings);

    // now until end of file, read lines and place data into spreadsheet

    int rowCount = 0;
    while (std::getline(tabResults, inputLine)) {
        std::istringstream is(inputLine);
        int col=0;
        spreadsheet->insertRow(rowCount);
        for (int i=0; i<colCount+2; i++) {
            std::string data;
            is >> data;
            if (i != 1) {
                QModelIndex index = spreadsheet->model()->index(rowCount, col);
                spreadsheet->model()->setData(index, data.c_str());
                col++;
            }
        }
        rowCount++;
    }
    tabResults.close();

    rowCount;
    spreadsheet->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(spreadsheet,SIGNAL(cellPressed(int,int)),this,SLOT(onSpreadsheetCellClicked(int,int)));

    //
    // create a chart, setting data points from first and last col of spreadsheet
    //

    chart = new QChart();
    chart->setAnimationOptions(QChart::AllAnimations);

    this->onSpreadsheetCellClicked(0,colCount-1);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->chart()->legend()->hide();

    //
    // into QWidget place chart and spreadsheet
    //

    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->addWidget(chartView, 1);
    layout->addWidget(spreadsheet, 1);

    //
    // add summary, detained info and spreadsheet with chart to the tabed widget
    //

    tabWidget->addTab(summary,tr("Summmary"));
    tabWidget->addTab(dakotaText, tr("General"));
    tabWidget->addTab(widget, tr("Data Values"));

    tabWidget->adjustSize();

    return 0;
}

void
DakotaResultsSampling::onSpreadsheetCellClicked(int row, int col)
{
    mLeft = spreadsheet->wasLeftKeyPressed();

    // create a new series
    chart->removeAllSeries();

    QScatterSeries *series = new QScatterSeries;

    int oldCol;
    if (mLeft == true) {
        oldCol= col2;
        col2 = col;
    } else {
        oldCol= col1;
        col1 = col;
    }

    int rowCount = spreadsheet->rowCount();
    //QLineSeries *series = new QLineSeries;
    for (int i=0; i<rowCount; i++) {
        QTableWidgetItem *itemX = spreadsheet->item(i,col1);
        QTableWidgetItem *itemY = spreadsheet->item(i,col2);
        QTableWidgetItem *itemOld = spreadsheet->item(i,oldCol);
        itemOld->setData(Qt::BackgroundRole, QColor(Qt::white));
        itemX->setData(Qt::BackgroundRole, QColor(Qt::lightGray));
        itemY->setData(Qt::BackgroundRole, QColor(Qt::lightGray));

        series->append(itemX->text().toDouble(), itemY->text().toDouble());
    }

    chart->addSeries(series);
    chart->createDefaultAxes();
}


extern QWidget *addLabeledLineEdit(QString theLabelName, QLineEdit **theLineEdit);

QWidget *
DakotaResultsSampling::createResultEDPWidget(QString &name, double mean, double stdDev) {
    QWidget *edp = new QWidget;
    QHBoxLayout *edpLayout = new QHBoxLayout();

    edp->setLayout(edpLayout);

    QLineEdit *nameLineEdit;
    QWidget *nameWidget = addLabeledLineEdit(QString("Name"), &nameLineEdit);
    nameLineEdit->setText(name);
    nameLineEdit->setDisabled(true);
    theNames.append(name);
    edpLayout->addWidget(nameWidget);

    QLineEdit *meanLineEdit;
    QWidget *meanWidget = addLabeledLineEdit(QString("Mean"), &meanLineEdit);
    meanLineEdit->setText(QString::number(mean));
    meanLineEdit->setDisabled(true);
    theMeans.append(mean);
    edpLayout->addWidget(meanWidget);

    QLineEdit *stdDevLineEdit;
    QWidget *stdDevWidget = addLabeledLineEdit(QString("StdDev"), &stdDevLineEdit);
    stdDevLineEdit->setText(QString::number(stdDev));
    stdDevLineEdit->setDisabled(true);
    theStdDevs.append(stdDev);
    edpLayout->addWidget(stdDevWidget);

    edpLayout->addStretch();

    return edp;
}