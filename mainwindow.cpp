/***************************************************************************
* Copyright (C) 2014 by Renaud Guezennec                                   *
* http://www.rolisteam.org/                                                *
*                                                                          *
*  This file is part of rcse                                               *
*                                                                          *
* rcse is free software; you can redistribute it and/or modify             *
* it under the terms of the GNU General Public License as published by     *
* the Free Software Foundation; either version 2 of the License, or        *
* (at your option) any later version.                                      *
*                                                                          *
* rcse is distributed in the hope that it will be useful,                  *
* but WITHOUT ANY WARRANTY; without even the implied warranty of           *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the             *
* GNU General Public License for more details.                             *
*                                                                          *
* You should have received a copy of the GNU General Public License        *
* along with this program; if not, write to the                            *
* Free Software Foundation, Inc.,                                          *
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.                 *
***************************************************************************/
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QMimeData>
#include <QUrl>
#include <QOpenGLWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QBuffer>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <QQmlError>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQmlComponent>
#include <QJsonArray>

#include "borderlisteditor.h"

#include "qmlhighlighter.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_currentPage(0)
{
    m_qmlGeneration =true;
    setAcceptDrops(true);
    ui->setupUi(this);

    Canvas* canvas = new Canvas();
    canvas->setCurrentPage(m_currentPage);

    m_canvasList.append(canvas);
    m_model = new FieldModel();
    ui->treeView->setModel(m_model);
    canvas->setModel(m_model);
    ui->treeView->setItemDelegateForColumn(CharacterSheetItem::BORDER,new BorderListEditor);
    m_view = new QGraphicsView();
    m_view->setAcceptDrops(true);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view->setViewport(new QOpenGLWidget());
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform );


    m_view->setScene(canvas);
    ui->scrollArea->setWidget(m_view);

    //ui->m_splitter->setStretchFactor(0,1);

    ui->m_addAct->setData(Canvas::ADD);
    ui->m_moveAct->setData(Canvas::MOVE);
    ui->m_deleteAct->setData(Canvas::DELETE);
    ui->m_addButtonAct->setData(Canvas::BUTTON);

    ui->m_addBtn->setDefaultAction(ui->m_addAct);
    ui->m_moveBtn->setDefaultAction(ui->m_moveAct);
    ui->m_deleteBtn->setDefaultAction(ui->m_deleteAct);
    ui->m_addButtonBtn->setDefaultAction(ui->m_addButtonAct);

    QmlHighlighter* highlighter = new QmlHighlighter(ui->m_codeEdit->document());


    connect(ui->m_addAct,SIGNAL(triggered(bool)),this,SLOT(setCurrentTool()));
    connect(ui->m_moveAct,SIGNAL(triggered(bool)),this,SLOT(setCurrentTool()));
    connect(ui->m_deleteAct,SIGNAL(triggered(bool)),this,SLOT(setCurrentTool()));
    connect(ui->m_addButtonAct,SIGNAL(triggered(bool)),this,SLOT(setCurrentTool()));


    connect(ui->actionQML_View,SIGNAL(triggered(bool)),this,SLOT(showQML()));
    connect(ui->actionCode_To_QML,SIGNAL(triggered(bool)),this,SLOT(showQMLFromCode()));

    connect(ui->m_saveAct,SIGNAL(triggered(bool)),this,SLOT(save()));
    connect(ui->actionSave_As,SIGNAL(triggered(bool)),this,SLOT(saveAs()));
    connect(ui->m_openAct,SIGNAL(triggered(bool)),this,SLOT(open()));



    connect(ui->m_addPage,SIGNAL(clicked(bool)),this,SLOT(addPage()));
    connect(ui->m_selectPageCb,SIGNAL(currentIndexChanged(int)),this,SLOT(currentPageChanged(int)));

    m_imgProvider = new RolisteamImageProvider();

   // ui->m_quickview->engine()->addImageProvider("rcs",m_imgProvider);
  //  ui->m_quickview->engine()->rootContext()->setContextProperty("_model",m_model);
  //  ui->m_quickview->setResizeMode(QQuickWidget::SizeRootObjectToView);

    connect(canvas,SIGNAL(imageChanged()),this,SLOT(setImage()));

    m_characterModel = new CharacterModel();
    ui->m_characterView->setModel(m_characterModel);

}


MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::setImage()
{
    int i = 0;
    m_pixList.clear();
    qDebug()<< "SetImage";
    for(auto canvas : m_canvasList)
    {
        m_imgProvider->insertPix(QStringLiteral("background_%1.jpg").arg(i),canvas->pixmap());
        m_pixList.append(canvas->pixmap());
        ++i;
    }
}

void MainWindow::setCurrentTool()
{
    QAction* action = dynamic_cast<QAction*>(sender());
    for(auto canvas : m_canvasList)
    {
        canvas->setCurrentTool((Canvas::Tool)action->data().toInt());
    }
}
void MainWindow::saveAs()
{
    m_filename = QFileDialog::getSaveFileName(this,tr("Save CharacterSheet"),QDir::homePath(),tr("Rolisteam CharacterSheet (*.rcs)"));
    if(!m_filename.isEmpty())
    {
        if(!m_filename.endsWith(".rcs"))
        {
            m_filename.append(QStringLiteral(".rcs"));
        }
        save();
    }
}
void MainWindow::save()
{
   // m_filename = QFileDialog::getSaveFileName(this,tr("Select file to export files"),QDir::homePath());
    if(m_filename.isEmpty())
        saveAs();
    else if(!m_filename.isEmpty())
    {
        if(!m_filename.endsWith(".rcs"))
        {
            m_filename.append(".rcs");
            ///@Warning
        }
        QFile file(m_filename);
        if(file.open(QIODevice::WriteOnly))
        {
            //init Json
            QJsonDocument json;
            QJsonObject obj;



            //Get datamodel
            QJsonObject data;
            m_model->save(data);
            obj["data"]=data;

            //qml file
            QString qmlFile=ui->m_codeEdit->document()->toPlainText();
            if(qmlFile.isEmpty())
            {
                generateQML(qmlFile);
            }
            obj["qml"]=qmlFile;


            //background
            QJsonArray images;
            for(auto canvas : m_canvasList)
            {
                QPixmap pix = canvas->pixmap();
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pix.save(&buffer, "PNG");
                images.append(QString(buffer.data().toBase64()));
            }
            obj["background"]=images;
            json.setObject(obj);
            file.write(json.toJson());

        }
        //
    }
}


void MainWindow::open()
{
    m_filename = QFileDialog::getOpenFileName(this,tr("Save CharacterSheet"),QDir::homePath(),tr("Rolisteam CharacterSheet (*.rcs)"));
    if(!m_filename.isEmpty())
    {
        QFile file(m_filename);
        if(file.open(QIODevice::ReadOnly))
        {
            QJsonDocument json = QJsonDocument::fromJson(file.readAll());
            QJsonObject jsonObj = json.object();
            QJsonObject data = jsonObj["data"].toObject();

            QString qml = jsonObj["qml"].toString();

            ui->m_codeEdit->setText(qml);

            QJsonArray images = jsonObj["background"].toArray();
            int i = 0;
            for(auto jsonpix : images)
            {
                QString str = jsonpix.toString();
                QByteArray array = QByteArray::fromBase64(str.toUtf8());
                QPixmap pix;
                pix.loadFromData(array);
                if(i!=0)
                {
                    Canvas* canvas = new Canvas();
                    m_canvasList.append(canvas);
                    connect(canvas,SIGNAL(imageChanged()),this,SLOT(setImage()));
                }
                m_imgProvider->insertPix(QStringLiteral("background_%1.jpg").arg(i),pix);
            }
            m_model->load(data,m_canvasList);
        }
    }
}
void MainWindow::updatePageSelector()
{
    QStringList list;
    ui->m_selectPageCb->clear();
    int i =0;
    for(Canvas* canvas: m_canvasList)
    {
        list << QStringLiteral("Page %1").arg(i+1);
        ++i;
    }
    ui->m_selectPageCb->addItems(list);
    ui->m_selectPageCb->setCurrentIndex(0);
}

void MainWindow::generateQML(QString& qml)
{
    QTextStream text(&qml);
    QPixmap pix;
    bool allTheSame=true;
    QSize size;
    for(QPixmap pix2 : m_pixList)
    {
        if(size != pix2.size())
        {
            if(size.isValid())
                allTheSame=false;
            size = pix2.size();
        }
        pix = pix2;

    }
   // QPixmap pix = m_canvasList.pixmap();
    if((allTheSame)&&(!pix.isNull()))
    {
        text << "import QtQuick 2.4\n";
        text << "import \"qrc:/resources/qml/\"\n";
        text << "\n";
        text << "Item {\n";
        text << "   id:root\n";
        text << "   focus: true\n";
        text << "   property int page: 0\n";
        text << "   property int maxPage:"<< m_canvasList.size()-1 <<"\n";
        text << "   onPageChanged: {\n";
        text << "       page=page>maxPage ? maxPage : page<0 ? 0 : page\n";
        text << "   }\n";
        text << "   Keys.onLeftPressed: --page\n";
        text << "   Keys.onRightPressed: ++page\n";
        text << "   signal rollDiceCmd(string cmd)\n";
        text << "   Image {\n";
        text << "       id:imagebg" << "\n";
        qreal ratio = (qreal)pix.width()/(qreal)pix.height();
        qreal ratioBis = (qreal)pix.height()/(qreal)pix.width();
        text << "       property real iratio :" << ratio << "\n";
        text << "       property real iratiobis :" << ratioBis << "\n";
        text << "       property real realscale: width/"<< pix.width() << "\n";
        text << "       width:(parent.width>parent.height*iratio)?iratio*parent.height:parent.width" << "\n";
        text << "       height:(parent.width>parent.height*iratio)?parent.height:iratiobis*parent.width" << "\n";
        text << "       source: \"image://rcs/background_%1.jpg\".arg(root.page)" << "\n";
        m_model->generateQML(text,CharacterSheetItem::FieldSec);
        text << "\n";
        text << "  }\n";
        text << "}\n";

     /*   text << "       Connections {\n";
        text << "           target: _model\n";
        text << "           onValuesChanged:{\n";
        m_model->generateQML(text,CharacterSheetItem::ConnectionSec);
        text << "       }\n";
        text << "   }\n";*/
        text.flush();

    }
}


void MainWindow::showQML()
{
    QString data;
    generateQML(data);
    ui->m_codeEdit->setText(data);

    QFile file("test.qml");
    if(file.open(QIODevice::WriteOnly))
    {
        file.write(data.toLatin1());
        file.close();
    }
    ui->m_quickview->engine()->clearComponentCache();
    ui->m_quickview->engine()->addImageProvider(QLatin1String("rcs"),m_imgProvider);
    QList<CharacterSheetItem *> list = m_model->children();
    for(CharacterSheetItem* item : list)
    {
        ui->m_quickview->engine()->rootContext()->setContextProperty(QStringLiteral("_%1").arg(item->getId()),item);
    }
    ui->m_quickview->setSource(QUrl::fromLocalFile("test.qml"));
    ui->m_quickview->setResizeMode(QQuickWidget::SizeRootObjectToView);
    QObject* root = ui->m_quickview->rootObject();
    connect(root,SIGNAL(rollDiceCmd(QString)),this,SLOT(rollDice(QString)));
}
void MainWindow::showQMLFromCode()
{
    QString data = ui->m_codeEdit->document()->toPlainText();

    QString name(QStringLiteral("test.qml"));
    if(QFile::exists(name))
    {
        QFile::remove(name);
    }
    QFile file(name);
    if(file.open(QIODevice::WriteOnly))
    {
        file.write(data.toLatin1());
        file.close();
    }

    //delete ui->m_quickview;
    ui->m_quickview->engine()->clearComponentCache();
    //ui->m_quickview->engine()->addImageProvider("rcs",m_imgProvider);
    ui->m_quickview->engine()->rootContext()->setContextProperty("_model",m_model);
    ui->m_quickview->setSource(QUrl::fromLocalFile(name));
    QObject* root = ui->m_quickview->rootObject();
    connect(root,SIGNAL(rollDiceCmd(QString)),this,SLOT(rollDice(QString)));
    ui->m_quickview->setResizeMode(QQuickWidget::SizeRootObjectToView);
}
void MainWindow::saveQML()
{
    QString qmlFile = QFileDialog::getOpenFileName(this,tr("Save CharacterSheet View"),QDir::homePath(),tr("CharacterSheet View (*.qml)"));
    if(!qmlFile.isEmpty())
    {
        QString data=ui->m_codeEdit->toPlainText();
        generateQML(data);
        ui->m_codeEdit->setText(data);

        QFile file(qmlFile);
        if(file.open(QIODevice::WriteOnly))
        {
            file.write(data.toLatin1());
            file.close();
        }
    }
}
void MainWindow::openQML()
{
    QString qmlFile = QFileDialog::getOpenFileName(this,tr("Save CharacterSheet View"),QDir::homePath(),tr("Rolisteam CharacterSheet View (*.qml)"));
    if(!qmlFile.isEmpty())
    {
        QFile file(m_filename);
        if(file.open(QIODevice::ReadOnly))
        {
            QString qmlContent = file.readAll();
            ui->m_codeEdit->setText(qmlContent);
            showQMLFromCode();

        }
    }
}
bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    return QMainWindow::eventFilter(obj,ev);
}
bool MainWindow::qmlGeneration() const
{
    return m_qmlGeneration;
}

void MainWindow::setQmlGeneration(bool qmlGeneration)
{
    m_qmlGeneration = qmlGeneration;
}

Field* MainWindow::addFieldAt(QPoint pos)
{
    qDebug() << "create Field";

    return NULL;
}
void MainWindow::rollDice(QString cmd)
{
    qDebug() << cmd;
}

void MainWindow::addPage()
{
    Canvas* previous = m_canvasList[m_currentPage];
    ++m_currentPage;
    Canvas* canvas = new Canvas();
    connect(canvas,SIGNAL(imageChanged()),this,SLOT(setImage()));

    canvas->setCurrentTool(previous->currentTool());

    canvas->setModel(m_model);
    m_canvasList.append(canvas);

    updatePageSelector();
    canvas->setCurrentPage(m_currentPage);
    currentPageChanged(m_currentPage);

}
void MainWindow::currentPageChanged(int i)
{
    m_view->setScene(m_canvasList[i]);
}
void MainWindow::removePage()
{

}
