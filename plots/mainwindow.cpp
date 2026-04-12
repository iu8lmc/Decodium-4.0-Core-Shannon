#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDesktopWidget>
#include <QScreen>
#include <QMessageBox>
#include <QMetaEnum>

#include <fstream>
#include <iostream>

using namespace std;
#define NPTS 3456

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  setGeometry(400, 250, 542, 390);
  
  setupPlot();
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::setupPlot()
{
  plotspec(ui->customPlot);
  setWindowTitle("Reference Spectrum");
  statusBar()->clearMessage();
  ui->customPlot->replot();
}

void MainWindow::plotspec(QCustomPlot *customPlot)
{
  QVector<double> x(NPTS), y1(NPTS), y2(NPTS), y3(NPTS), y4(NPTS);
  ifstream inFile;
  double ymin=1.0e30;
  double ymax=-ymin;
  inFile.open("c:/users/joe/appdata/local/WSJT-X/refspec.dat");

  for (int i = 0; i < NPTS; i++) {
    inFile >> x[i] >> y1[i] >> y2[i] >> y3[3] >> y4[4];
    if(y2[i]>ymax) ymax=y2[i];
    if(y2[i]<ymin) ymin=y2[i];
  }

  customPlot->addGraph();
  customPlot->graph(0)->setData(x, y2);
  customPlot->xAxis->setLabel("Frequency (Hz)");
  customPlot->yAxis->setLabel("Relative power (dB)");
  customPlot->xAxis->setRange(0, 6000);
  customPlot->yAxis->setRange(ymin, ymax);
}
