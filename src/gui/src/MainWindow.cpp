#include "MainWindow.h"
#include "RpcClient.h"
#include "TelemetryWorker.h"
#include "widgets/FanTile.h"
#include "widgets/CollapsiblePanel.h"

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

// ---- Deep-blue theme close to FanControl look ----
static void applyBlueTheme(bool dark) {
    QPalette pal;
    if (dark) {
        pal.setColor(QPalette::Window, QColor("#0e1b2a"));   // deep blue bg
        pal.setColor(QPalette::Base,   QColor("#13273d"));   // cards base
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
    // Blue tile
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

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent) {
    rpc_ = new RpcClient();
    tw_  = new TelemetryWorker(rpc_, this);

    // ---- Toolbar ----
    auto* tb = addToolBar("toolbar");
    actDetect_  = new QAction("Detect", this);
    actRefresh_ = new QAction("Refresh", this);
    actStart_   = new QAction("Start", this);
    actStop_    = new QAction("Stop", this);
    actTheme_   = new QAction("Light Mode", this);

    connect(actDetect_,  &QAction::triggered, this, &MainWindow::detect);
    connect(actRefresh_, &QAction::triggered, this, &MainWindow::refresh);
    connect(actStart_,   &QAction::triggered, this, &MainWindow::startEngine);
    connect(actStop_,    &QAction::triggered, this, &MainWindow::stopEngine);
    connect(actTheme_,   &QAction::triggered, this, &MainWindow::switchTheme);

    tb->addAction(actDetect_);
    tb->addAction(actRefresh_);
    tb->addSeparator();
    tb->addAction(actStart_);
    tb->addAction(actStop_);
    tb->addSeparator();
    tb->addAction(actTheme_);

    // ---- Central single layout (no splitter) ----
    auto* central = new QWidget(this);
    auto* v = new QVBoxLayout(central);
    v->setContentsMargins(12,12,12,12);
    v->setSpacing(12);
    setCentralWidget(central);

    // TOP: draggable tiles (compact)
    channelsList_ = new QListWidget(this);
    channelsList_->setViewMode(QListView::IconMode);
    channelsList_->setMovement(QListView::Snap);
    channelsList_->setDragDropMode(QAbstractItemView::InternalMove);
    channelsList_->setResizeMode(QListView::Adjust);
    channelsList_->setWrapping(true);
    channelsList_->setSpacing(10);
    channelsList_->setGridSize(QSize(280, 110)); // compact like FanControl
    v->addWidget(channelsList_);

    // SENSORS: collapsible panel (hidden by default)
    auto* sensorsPanel = new CollapsiblePanel("Sensors", this);
    sensorsPanel_ = sensorsPanel;
    sensorsPanel_->setVisible(false); // hidden until user opens via Detect/Refresh result or future toggle
    QWidget* body = sensorsPanel->body();
    auto* vb = qobject_cast<QVBoxLayout*>(body->layout());
    if (!vb) vb = new QVBoxLayout(body);
    btnApplyHide_ = new QPushButton("Apply Hide", body);
    connect(btnApplyHide_, &QPushButton::clicked, this, &MainWindow::applyHideSensors);
    vb->addWidget(btnApplyHide_, 0, Qt::AlignLeft);
    auto* cont = new QWidget(body);
    sensorsGrid_ = new QGridLayout(cont);
    sensorsGrid_->setContentsMargins(0,0,0,0);
    sensorsGrid_->setHorizontalSpacing(10);
    sensorsGrid_->setVerticalSpacing(10);
    vb->addWidget(cont);
    v->addWidget(sensorsPanel_);

    // BOTTOM (Curves/Mix/Triggers): start hidden unless we have channels
    curvesArea_ = new QScrollArea(this);
    curvesArea_->setWidgetResizable(true);
    curvesWrap_ = new QWidget(curvesArea_);
    curvesGrid_ = new QGridLayout(curvesWrap_);
    curvesGrid_->setContentsMargins(0,0,0,0);
    curvesGrid_->setHorizontalSpacing(10);
    curvesGrid_->setVerticalSpacing(10);
    curvesArea_->setWidget(curvesWrap_);
    curvesArea_->setVisible(false);
    v->addWidget(curvesArea_);

    resize(1280, 900);
    applyBlueTheme(true); // start in dark blue
    actTheme_->setText("Light Mode");

    connect(tw_, &TelemetryWorker::tickReady, this, &MainWindow::onTelemetry);
    tw_->start(1000);

    QTimer::singleShot(120, this, &MainWindow::refresh);
}

MainWindow::~MainWindow() = default;

void MainWindow::switchTheme() {
    isDark_ = !isDark_;
    applyBlueTheme(isDark_);
    actTheme_->setText(isDark_ ? "Light Mode" : "Dark Mode");

    // re-apply tile stylesheet for all tiles
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
    // rebuild grid skipping hiddenSensors_
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

        // small checkbox row
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

static QWidget* makeFanTileWidget(const QJsonObject& ch, bool dark) {
    auto* tile = new FanTile();
    tile->setObjectName("fanTile");
    tile->setStyleSheet(tileStyle(dark));
    tile->setTitle(ch.value("name").toString());
    tile->setSensor(ch.value("sensor").toString());
    return tile;
}

void MainWindow::rebuildChannels(const QJsonArray& channels) {
    channelsList_->clear();
    chItems_.clear();

    for (const auto& it : channels) {
        auto obj = it.toObject();
        const QString id = obj.value("id").toString();

        auto* tile = makeFanTileWidget(obj, isDark_);

        auto* item = new QListWidgetItem();
        item->setSizeHint(QSize(270, 100));
        item->setData(Qt::UserRole, id);
        channelsList_->addItem(item);
        channelsList_->setItemWidget(item, tile);

        // store refs for telemetry
        ChannelCardRefs refs; refs.card = tile;
        chCards_[id] = refs;
    }

    // Show curves panel only if we have channels configured
    curvesArea_->setVisible(channels.size() > 0);
}

void MainWindow::rebuildSensors(const QJsonArray& sensors) {
    sensorsCache_ = sensors;
    // keep sensors panel hidden by default; user can show via future toggle, or we show it after Detect
    // Nothing to do here unless you want it visible: sensorsPanel_->setVisible(true);
}

void MainWindow::rebuildCurves(const QJsonArray& channels) {
    // placeholder: keep hidden unless channels available
    if (channels.isEmpty()) return;
    // minimal placeholder tiles removed to keep lower area compact initially
}

QString MainWindow::chooseSensorForPwm(const QString& pwmLabel, const QJsonArray& sensors) const {
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
    // Run daemon-side full auto-setup (perturbation+calibration)
    auto res = rpc_->call("detectCalibrate");
    if (!res.contains("result")) {
        statusBar()->showMessage("detectCalibrate failed", 1800);
        return;
    }
    auto r = res["result"].toObject();
    auto sensors = r.value("sensors").toArray();
    auto pwms    = r.value("pwms").toArray();
    auto mapping = r.value("mapping").toObject();

    // create channels batch based on mapping
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
        statusBar()->showMessage("Detection done, engine started", 1800);
    }

    // Update UI caches; show sensors panel so user can hide some
    sensorsPanel_->setVisible(true);
    rebuildSensors(sensors);
    refresh();
}

void MainWindow::refresh() {
    auto e = rpc_->enumerate();
    if (e.contains("result")) {
        auto r = e["result"].toObject();
        rebuildSensors(r.value("sensors").toArray());
        auto ch = rpc_->listChannels();
        if (ch.isEmpty()) {
            // No config → run auto-setup once
            detect();
            return;
        }
        rebuildChannels(ch);
    } else {
        statusBar()->showMessage("daemon not reachable", 1500);
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
