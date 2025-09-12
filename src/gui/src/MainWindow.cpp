#include "MainWindow.h"
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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // Basic UI scaffold (keeps previous structure intact)
    auto* tb = addToolBar("toolbar");
    auto* actDark = new QAction("Dark / Light", this);
    tb->addAction(actDark);

    splitter_ = new QSplitter(this);
    setCentralWidget(splitter_);

    // Channels area
    saChannels_ = new QScrollArea(this);
    wrapChannels_ = new QWidget(this);
    saChannels_->setWidgetResizable(true);
    saChannels_->setWidget(wrapChannels_);
    splitter_->addWidget(saChannels_);

    // Status
    statusEngine_ = new QLabel("Engine: stopped", this);
    statusBar()->addPermanentWidget(statusEngine_);

    // Language switch (stub)
    comboLang_ = new QComboBox(this);
    comboLang_->addItems({"en","de"});
    statusBar()->addPermanentWidget(new QLabel("Lang:", this));
    statusBar()->addPermanentWidget(comboLang_);

    connect(comboLang_, &QComboBox::currentTextChanged, this, &MainWindow::onSwitchLang);
    connect(actDark, &QAction::triggered, this, [this]{
        // Simple toggle: Fusion dark vs light
        static bool dark=false; dark = !dark;
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

    retranslate();
}

MainWindow::~MainWindow() = default;

// --- stubs (fill with real logic later, keep linker happy) ---
void MainWindow::retranslate() {}
void MainWindow::onSwitchLang(const QString&) {}
void MainWindow::rebuildChannelCards() {}
void MainWindow::rebuildSources() {}
void MainWindow::refreshAll() {}
void MainWindow::startTelemetry() {}
void MainWindow::onEngine(bool) {}
void MainWindow::onOpenDetect() {}
