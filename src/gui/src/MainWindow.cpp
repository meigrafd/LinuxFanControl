#include "MainWindow.h"
#include "RpcClient.h"

#include <QToolBar>
#include <QAction>
#include <QApplication>
#include <QStatusBar>
#include <QSplitter>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QStyleFactory>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent)
{
    rpc_ = new RpcClient();

    buildUi();

    // Initial refresh after UI is up
    QTimer::singleShot(100, this, &MainWindow::refresh);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    auto* tb = addToolBar("toolbar");

    auto* actRefresh = new QAction("Refresh", this);
    connect(actRefresh, &QAction::triggered, this, &MainWindow::refresh);
    tb->addAction(actRefresh);

    auto* actDark = new QAction("Dark / Light", this);
    connect(actDark, &QAction::triggered, this, [this]{
        static bool dark=false; dark=!dark;
        if (dark) {
            QPalette pal;
            pal.setColor(QPalette::Window, QColor(30,30,30));
            pal.setColor(QPalette::WindowText, Qt::white);
            QApplication::setStyle("Fusion");
            qApp->setPalette(pal);
        } else {
            QApplication::setStyle(QStyleFactory::create("Fusion"));
            qApp->setPalette(qApp->style()->standardPalette());
        }
    });
    tb->addAction(actDark);

    splitter_ = new QSplitter(this);
    splitter_->setOrientation(Qt::Vertical);
    setCentralWidget(splitter_);

    // TOP (PWMs)
    saTop_ = new QScrollArea(this);
    saTop_->setWidgetResizable(true);
    wrapTop_ = new QWidget(this);
    gridTop_ = new QGridLayout(wrapTop_);
    gridTop_->setContentsMargins(8,8,8,8);
    gridTop_->setHorizontalSpacing(10);
    gridTop_->setVerticalSpacing(10);
    saTop_->setWidget(wrapTop_);
    splitter_->addWidget(saTop_);

    // BOTTOM (Sensors)
    saBottom_ = new QScrollArea(this);
    saBottom_->setWidgetResizable(true);
    wrapBottom_ = new QWidget(this);
    gridBottom_ = new QGridLayout(wrapBottom_);
    gridBottom_->setContentsMargins(8,8,8,8);
    gridBottom_->setHorizontalSpacing(10);
    gridBottom_->setVerticalSpacing(10);
    saBottom_->setWidget(wrapBottom_);
    splitter_->addWidget(saBottom_);

    statusEngine_ = new QLabel("Ready", this);
    statusBar()->addPermanentWidget(statusEngine_);

    comboLang_ = new QComboBox(this);
    comboLang_->addItems({"en","de"});
    statusBar()->addPermanentWidget(new QLabel("Lang:", this));
    statusBar()->addPermanentWidget(comboLang_);
    connect(comboLang_, &QComboBox::currentTextChanged, this, &MainWindow::onSwitchLang);
}

void MainWindow::clearGrid(QGridLayout* g) {
    while (auto* item = g->takeAt(0)) {
        if (auto* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
}

QWidget* MainWindow::makePwmCard(const QJsonObject& p) {
    auto* card = new QFrame;
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Raised);
    auto* v = new QVBoxLayout(card);
    auto label = p.value("label").toString();
    auto pwm   = p.value("pwm").toString();
    auto en    = p.value("enable").toString();
    auto tach  = p.value("tach").toString();
    v->addWidget(new QLabel(QString("<b>%1</b>").arg(label)));
    v->addWidget(new QLabel(QString("pwm: %1").arg(pwm)));
    if (!en.isEmpty())   v->addWidget(new QLabel(QString("enable: %1").arg(en)));
    if (!tach.isEmpty()) v->addWidget(new QLabel(QString("tach: %1").arg(tach)));
    return card;
}

QWidget* MainWindow::makeSensorCard(const QJsonObject& s) {
    auto* card = new QFrame;
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Raised);
    auto* v = new QVBoxLayout(card);
    auto label = s.value("label").toString();
    auto path  = s.value("path").toString();
    auto type  = s.value("type").toString("Unknown");
    v->addWidget(new QLabel(QString("<b>%1</b>").arg(label)));
    v->addWidget(new QLabel(QString("path: %1").arg(path)));
    v->addWidget(new QLabel(QString("type: %1").arg(type)));
    return card;
}

void MainWindow::refresh() {
    // Version probe (optional)
    auto v = rpc_->version();
    if (v.contains("result")) {
        auto r = v["result"].toObject();
        statusEngine_->setText(QString("%1 %2").arg(r.value("name").toString(), r.value("version").toString()));
    } else {
        statusEngine_->setText("daemon: not reachable");
    }

    // Enumerate
    auto e = rpc_->enumerate();
    if (!e.contains("result")) return;
    auto r = e["result"].toObject();
    auto sensors = r.value("sensors").toArray();
    auto pwms    = r.value("pwms").toArray();

    clearGrid(gridTop_);
    clearGrid(gridBottom_);

    // place cards in 3 columns
    int cols = 3;
    int row=0, col=0;
    for (const auto& it : pwms) {
        auto obj = it.toObject();
        auto* w = makePwmCard(obj);
        gridTop_->addWidget(w, row, col);
        if (++col >= cols) { col=0; ++row; }
    }
    row = 0; col = 0;
    for (const auto& it : sensors) {
        auto obj = it.toObject();
        auto* w = makeSensorCard(obj);
        gridBottom_->addWidget(w, row, col);
        if (++col >= cols) { col=0; ++row; }
    }
}

void MainWindow::onSwitchLang(const QString& /*code*/) {
    // placeholder for later i18n-switch; current UI uses static strings
}
