#include "MainWindow.h"
#include "RpcClient.h"
#include "dialogs/DetectDialog.h"
#include "widgets/FanTile.h"
#include "telemetry/ShmSubscriber.h"
#include "import/FanControlImporter.h"

#include <QToolBar>
#include <QAction>
#include <QApplication>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QListView>
#include <QAbstractItemView>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QPalette>
#include <QFileDialog>
#include <QMessageBox>

// Theme helpers (unchanged)
static void applyBlueTheme(bool dark) {
    QPalette pal;
    if (dark) {
        pal.setColor(QPalette::Window, QColor("#0e1b2a"));
        pal.setColor(QPalette::Base,   QColor("#13273d"));
        pal.setColor(QPalette::AlternateBase, QColor("#0e1b2a"));
        pal.setColor(QPalette::Button, QColor("#1b3a5b"));
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Highlight, QColor("#3a7bd5"));
        pal.setColor(QPalette::HighlightedText, Qt::white);
    } else {
        pal = qApp->style()->standardPalette();
    }
    qApp->setPalette(pal);
}
static QString tileStyle(bool dark) {
    if (dark) {
        return QStringLiteral(
            "QWidget#fanTile {"
            "  background: #1a3250;"
            "  border: 1px solid #2a4a75;"
            "  border-radius: 10px;"
            "}"
            "QPushButton {"
            "  background:#2a4a75; color:white; border:0px; padding:4px 8px; border-radius:5px;"
            "}"
            "QPushButton:hover { background:#35609b; }"
            "QLabel { color:#e8f0ff; }"
            "QLabel#title { font-weight:600; }"
        );
    } else {
        return QStringLiteral(
            "QWidget#fanTile {"
            "  background: #eaf1ff;"
            "  border: 1px solid #c9d7ff;"
            "  border-radius: 10px;"
            "}"
            "QPushButton {"
            "  background:#d2e0ff; color:#123; border:0px; padding:4px 8px; border-radius:5px;"
            "}"
            "QPushButton:hover { background:#bcd1ff; }"
            "QLabel { color:#123; }"
            "QLabel#title { font-weight:600; }"
        );
    }
}

// MainWindow
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    rpc_ = new RpcClient();
    shm_ = new ShmSubscriber(this);

    auto* tb = addToolBar("toolbar");
    actSetup_   = new QAction("Setup", this);
    actImport_  = new QAction("Import…", this);
    actRefresh_ = new QAction("Refresh", this);
    actStart_   = new QAction("Start", this);
    actStop_    = new QAction("Stop", this);
    actTheme_   = new QAction("Light Mode", this);

    connect(actSetup_,   &QAction::triggered, this, &MainWindow::detect);
    connect(actImport_,  &QAction::triggered, this, &MainWindow::onImport);
    connect(actRefresh_, &QAction::triggered, this, &MainWindow::refresh);
    connect(actStart_,   &QAction::triggered, this, &MainWindow::startEngine);
    connect(actStop_,    &QAction::triggered, this, &MainWindow::stopEngine);
    connect(actTheme_,   &QAction::triggered, this, &MainWindow::switchTheme);

    tb->addAction(actSetup_);
    tb->addAction(actImport_);
    tb->addAction(actRefresh_);
    tb->addSeparator();
    tb->addAction(actStart_);
    tb->addAction(actStop_);
    tb->addSeparator();
    tb->addAction(actTheme_);

    // Central
    auto* central = new QWidget(this);
    auto* v = new QVBoxLayout(central);
    v->setContentsMargins(12,12,12,12);
    v->setSpacing(12);
    setCentralWidget(central);

    // Empty state
    emptyState_ = new QWidget(central);
    auto* ev = new QVBoxLayout(emptyState_);
    ev->setContentsMargins(20,40,20,40);
    ev->setSpacing(12);
    auto* title = new QLabel("<b>No channels yet</b>", emptyState_);
    auto* desc  = new QLabel("Click <i>Setup</i> to detect sensors and calibrate fans, or <i>Import</i>.", emptyState_);
    btnEmptySetup_ = new QPushButton("Setup", emptyState_);
    btnEmptySetup_->setFixedWidth(120);
    ev->addWidget(title, 0, Qt::AlignHCenter);
    ev->addWidget(desc, 0, Qt::AlignHCenter);
    ev->addWidget(btnEmptySetup_, 0, Qt::AlignHCenter);
    connect(btnEmptySetup_, &QPushButton::clicked, this, &MainWindow::detect);
    v->addWidget(emptyState_);

    // TOP tiles
    channelsList_ = new QListWidget(this);
    channelsList_->setViewMode(QListView::IconMode);
    channelsList_->setMovement(QListView::Snap);
    channelsList_->setDragDropMode(QAbstractItemView::InternalMove);
    channelsList_->setResizeMode(QListView::Adjust);
    channelsList_->setWrapping(true);
    channelsList_->setSpacing(10);
    channelsList_->setGridSize(QSize(280, 110));
    v->addWidget(channelsList_);

    // Sensors hidden by default
    sensorsPanel_ = new QWidget(this);
    auto* spLay = new QVBoxLayout(sensorsPanel_);
    spLay->setContentsMargins(0,0,0,0);
    spLay->setSpacing(8);
    btnApplyHide_ = new QPushButton("Apply Hide", sensorsPanel_);
    connect(btnApplyHide_, &QPushButton::clicked, this, &MainWindow::applyHideSensors);
    spLay->addWidget(btnApplyHide_, 0, Qt::AlignLeft);
    auto* cont = new QWidget(sensorsPanel_);
    sensorsGrid_ = new QGridLayout(cont);
    sensorsGrid_->setContentsMargins(0,0,0,0);
    sensorsGrid_->setHorizontalSpacing(10);
    sensorsGrid_->setVerticalSpacing(10);
    spLay->addWidget(cont);
    sensorsPanel_->setVisible(false);
    v->addWidget(sensorsPanel_);

    resize(1280, 900);
    applyBlueTheme(true);
    actTheme_->setText("Light Mode");

    connect(shm_, &ShmSubscriber::tickReady, this, &MainWindow::onTelemetry);
    shm_->start("/lfc_telemetry", 200);

    QTimer::singleShot(100, this, &MainWindow::refresh);
}

MainWindow::~MainWindow() {
    if (shm_) shm_->stop();
}

void MainWindow::switchTheme() {
    isDark_ = !isDark_;
    applyBlueTheme(isDark_);
    actTheme_->setText(isDark_ ? "Light Mode" : "Dark Mode");
    for (int i = 0; i < channelsList_->count(); ++i) {
        if (auto* w = channelsList_->itemWidget(channelsList_->item(i))) {
            if (w->objectName() == "fanTile") {
                w->setStyleSheet(tileStyle(isDark_));
            }
        }
    }
}

void MainWindow::startEngine() {
    rpc_->call("engineStart");
    statusBar()->showMessage("Engine started", 1200);
}
void MainWindow::stopEngine() {
    rpc_->call("engineStop");
    statusBar()->showMessage("Engine stopped", 1200);
}

void MainWindow::applyHideSensors() {
    for (int i = sensorsGrid_->count() - 1; i >= 0; --i) {
        auto* it = sensorsGrid_->takeAt(i);
        if (auto* w = it->widget()) { w->hide(); w->deleteLater(); }
        delete it;
    }
    sensorCards_.clear();

    int cols = 4, row=0, col=0;
    for (const auto& v : sensorsCache_) {
        const auto s = v.toObject();
        QString label = s.value("label").toString();
        if (hiddenSensors_.contains(label)) continue;

        auto* roww = new QWidget;
        roww->setObjectName("sensorRow");
        auto* h = new QHBoxLayout(roww);
        h->setContentsMargins(8,6,8,6);
        h->setSpacing(6);
        auto* chk = new QCheckBox(roww);
        chk->setChecked(true);
        connect(chk, &QCheckBox::toggled, this, [this, label](bool on){ if (on) hiddenSensors_.remove(label); else hiddenSensors_.insert(label); });
        h->addWidget(chk);
        h->addWidget(new QLabel(label, roww), 1);

        sensorsGrid_->addWidget(roww, row, col);
        if (++col >= cols) { col=0; ++row; }
    }
}

void MainWindow::showEmptyState(bool on) {
    emptyState_->setVisible(on);
    channelsList_->setVisible(!on);
}

QWidget* MainWindow::makeFanTileWidget(const QJsonObject& ch) {
    auto* tile = new FanTile(this);
    tile->setObjectName("fanTile");
    tile->setStyleSheet(tileStyle(isDark_));
    tile->setTitle(ch.value("name").toString());
    tile->setSensor(ch.value("sensor").toString());
    return tile;
}

void MainWindow::rebuildChannels(const QJsonArray& channels) {
    channelsList_->clear();
    chItems_.clear();
    chCards_.clear();

    showEmptyState(channels.isEmpty());

    for (const auto& it : channels) {
        auto obj = it.toObject();
        const QString id = obj.value("id").toString();

        auto* tile = makeFanTileWidget(obj);

        auto* item = new QListWidgetItem();
        item->setSizeHint(QSize(270, 100));
        item->setData(Qt::UserRole, id);
        channelsList_->addItem(item);
        channelsList_->setItemWidget(item, tile);

        ChannelCardRefs refs; refs.card = tile;
        chCards_[id] = refs;
        chItems_.insert(id, item);
    }
}

void MainWindow::rebuildSensors(const QJsonArray& sensors) {
    sensorsCache_ = sensors;
}

QString MainWindow::chooseSensorForPwm(const QJsonArray& sensors, const QString& pwmLabel) const {
    auto pickByType = [&](const QString& type) -> QString {
        for (const auto& it : sensors) {
            const auto s = it.toObject();
            if (s.value("type").toString().compare(type, Qt::CaseInsensitive) == 0) {
                return s.value("path").toString();
            }
        }
        return {};
    };
    if (pwmLabel.contains("amdgpu", Qt::CaseInsensitive))
        if (auto p = pickByType("GPU"); !p.isEmpty()) return p;
        if (pwmLabel.contains("k10temp", Qt::CaseInsensitive) || pwmLabel.contains("coretemp", Qt::CaseInsensitive))
            if (auto p = pickByType("CPU"); !p.isEmpty()) return p;
            if (!sensors.isEmpty()) return sensors.first().toObject().value("path").toString();
            return {};
}

void MainWindow::detect() {
    DetectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        statusBar()->showMessage("Setup cancelled", 1500);
        return;
    }
    auto res = dlg.result();
    if (res.isEmpty()) {
        statusBar()->showMessage("Setup failed", 1500);
        return;
    }

    auto sensors = res.value("sensors").toArray();
    auto pwms    = res.value("pwms").toArray();
    auto mapping = res.value("mapping").toObject();

    // Create channels in batch
    QJsonArray batch;
    int idCounter = 1;
    for (const auto& it : pwms) {
        const auto p = it.toObject();
        const QString lbl = p.value("label").toString();
        const auto m = mapping.value(lbl).toObject();
        const QString sensorPath = m.value("sensor_path").toString();
        if (sensorPath.isEmpty()) continue;

        QJsonObject params;
        params["name"]   = lbl;
        params["sensor"] = sensorPath;
        params["pwm"]    = p.value("pwm").toString();

        batch.push_back(QJsonObject{
            {"jsonrpc","2.0"},
            {"method","createChannel"},
            {"id", QString::number(idCounter++)},
                        {"params", params}
        });
    }
    if (!batch.isEmpty()) {
        rpc_->callBatch(batch);
        rpc_->call("engineStart");
        statusBar()->showMessage("Setup completed, engine started", 1800);
    }

    sensorsPanel_->setVisible(true);
    refresh();
}

void MainWindow::onImport() {
    const QString path = QFileDialog::getOpenFileName(this, "Import FanControl JSON", QString(), "JSON (*.json)");
    if (path.isEmpty()) return;
    QString err;
    if (!Importer::importFanControlJson(rpc_, path, &err)) {
        QMessageBox::warning(this, "Import failed", err.isEmpty()? "Unknown error" : err);
        return;
    }
    statusBar()->showMessage("Import completed", 1800);
    refresh();
}

void MainWindow::refresh() {
    auto e = rpc_->enumerate();
    if (e.contains("result")) {
        auto r = e["result"].toObject();
        rebuildSensors(r.value("sensors").toArray());

        auto ch = rpc_->listChannels();
        rebuildChannels(ch);
    } else {
        statusBar()->showMessage("daemon not reachable", 1500);
        showEmptyState(true);
    }
}

void MainWindow::onTelemetry(const QJsonArray& channels) {
    for (const auto& it : channels) {
        const auto ch = it.toObject();
        const QString id   = ch.value("id").toString();
        const double lastOut  = ch.contains("last_out")  ? ch["last_out"].toDouble()  : NAN;
        const double lastTemp = ch.contains("last_temp") ? ch["last_temp"].toDouble() : NAN;

        auto* item = chItems_.value(id, nullptr);
        if (!item) continue;
        auto* w = channelsList_->itemWidget(item);
        if (auto* tile = qobject_cast<FanTile*>(w)) {
            if (std::isfinite(lastOut))  tile->setDuty(lastOut);
            if (std::isfinite(lastTemp)) tile->setTemp(lastTemp);
        }
    }
}
