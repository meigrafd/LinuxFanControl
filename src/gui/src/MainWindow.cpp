#include "MainWindow.h"
#include "RpcClient.h"
#include "TelemetryWorker.h"
#include "widgets/FanTile.h"
#include "widgets/CollapsiblePanel.h"

#include <QToolBar>
#include <QAction>
#include <QApplication>
#include <QStatusBar>
#include <QSplitter>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QStyleFactory>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTimer>
#include <QMetaObject>
#include <QCheckBox>
#include <QPalette>
#include <QListWidget>
#include <QListView>
#include <QAbstractItemView>

static QString kCardStyle(bool dark) {
    if (dark) {
        return QStringLiteral(
            "QFrame {"
            "  background:#2f2f2f;"
            "  border:1px solid #444;"
            "  border-radius:14px;"
            "  padding:10px;"
            "}"
            "QLabel { color:#e8e8e8; font-size:14px; }"
            "QLabel#tileTitle { font-weight:600; font-size:16px; }");
    } else {
        return QStringLiteral(
            "QFrame {"
            "  background:#ffffff;"
            "  border:1px solid #d0d0d0;"
            "  border-radius:14px;"
            "  padding:10px;"
            "}"
            "QLabel { color:#222; font-size:14px; }"
            "QLabel#tileTitle { font-weight:600; font-size:16px; }");
    }
}

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent) {
    rpc_ = new RpcClient();
    tw_  = new TelemetryWorker(rpc_, this);

    buildUi();
    resize(1280, 900);

    connect(tw_, &TelemetryWorker::tickReady, this, &MainWindow::onTelemetry);
    tw_->start(1000);

    QTimer::singleShot(120, this, &MainWindow::refresh);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
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

    splitter_ = new QSplitter(this);
    splitter_->setOrientation(Qt::Vertical);
    setCentralWidget(splitter_);

    // TOP: draggable tiles (channels)
    channelsList_ = new QListWidget(this);
    channelsList_->setViewMode(QListView::IconMode);
    channelsList_->setMovement(QListView::Snap);
    channelsList_->setDragDropMode(QAbstractItemView::InternalMove);
    channelsList_->setResizeMode(QListView::Adjust);
    channelsList_->setWrapping(true);
    channelsList_->setSpacing(14);
    channelsList_->setGridSize(QSize(360, 150)); // outer footprint incl. margins
    splitter_->addWidget(channelsList_);

    // CENTER: collapsible sensors panel
    auto* sensorsPanel = new CollapsiblePanel("Sensors", this);
    sensorsPanel_ = sensorsPanel;
    QWidget* body = sensorsPanel->body();
    auto* vb = qobject_cast<QVBoxLayout*>(body->layout());
    if (!vb) vb = new QVBoxLayout(body);
    btnApplyHide_ = new QPushButton("Apply Hide", body);
    btnApplyHide_->setToolTip("Hide all unchecked sensors from the overview");
    connect(btnApplyHide_, &QPushButton::clicked, this, &MainWindow::applyHideSensors);
    vb->addWidget(btnApplyHide_, 0, Qt::AlignLeft);

    auto* cont = new QWidget(body);
    sensorsGrid_ = new QGridLayout(cont);
    sensorsGrid_->setContentsMargins(12,12,12,12);
    sensorsGrid_->setHorizontalSpacing(12);
    sensorsGrid_->setVerticalSpacing(12);
    vb->addWidget(cont);

    splitter_->addWidget(sensorsPanel);

    // BOTTOM: curves/triggers/mix tiles
    curvesArea_ = new QScrollArea(this);
    curvesArea_->setWidgetResizable(true);
    curvesWrap_ = new QWidget(curvesArea_);
    curvesGrid_ = new QGridLayout(curvesWrap_);
    curvesGrid_->setContentsMargins(12,12,12,12);
    curvesGrid_->setHorizontalSpacing(12);
    curvesGrid_->setVerticalSpacing(12);
    curvesArea_->setWidget(curvesWrap_);
    splitter_->addWidget(curvesArea_);

    statusBar()->showMessage("Ready", 1500);
}

void MainWindow::switchTheme() {
    isDark_ = !isDark_;
    if (isDark_) {
        QPalette pal;
        pal.setColor(QPalette::Window, QColor(30, 30, 30));
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Base, QColor(20,20,20));
        pal.setColor(QPalette::AlternateBase, QColor(35,35,35));
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::Button, QColor(45,45,45));
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::Highlight, QColor(64,128,255));
        qApp->setPalette(pal);
        actTheme_->setText("Light Mode");
    } else {
        qApp->setPalette(qApp->style()->standardPalette());
        actTheme_->setText("Dark Mode");
    }

    // restyle sensor tiles
    for (auto it = sensorCards_.begin(); it != sensorCards_.end(); ++it) {
        if (it.value()) it.value()->setStyleSheet(kCardStyle(isDark_));
    }
    // restyle curve tiles
    for (int i=0; i<curvesGrid_->count(); ++i) {
        if (auto* w = curvesGrid_->itemAt(i)->widget())
            w->setStyleSheet(kCardStyle(isDark_));
    }
    // channel tiles inherit app palette; (FanTile has neutral palette-based bg)
}

QWidget* MainWindow::makeSensorTile(const QJsonObject& s, bool checked) {
    auto* card = new QFrame;
    card->setObjectName("sensorCard");
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Raised);
    card->setStyleSheet(kCardStyle(isDark_));

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(12,10,12,10);
    v->setSpacing(6);
    const QString label = s.value("label").toString();
    const QString path  = s.value("path").toString();
    const QString type  = s.value("type").toString("Unknown");

    auto* top = new QHBoxLayout();
    auto* chk = new QCheckBox(card);
    chk->setChecked(checked);
    chk->setToolTip("Check to keep this sensor visible. Click 'Apply Hide' to hide unchecked.");
    top->addWidget(chk);
    auto* name = new QLabel(QString("<b>%1</b>").arg(label), card);
    name->setObjectName("tileTitle");
    name->setTextFormat(Qt::RichText);
    top->addWidget(name, 1);
    v->addLayout(top);

    v->addWidget(new QLabel(QString("type: %1").arg(type), card));
    v->addWidget(new QLabel(QString("path: %1").arg(path), card));

    card->setProperty("sensorLabel", label);
    chk->setProperty("sensorLabel", label);
    connect(chk, &QCheckBox::toggled, this, [this, label](bool on){
        if (on) hiddenSensors_.remove(label);
        else    hiddenSensors_.insert(label);
    });
        return card;
}

QWidget* MainWindow::makeCurveTile(const QString& title) {
    auto* card = new QFrame;
    card->setObjectName("curveCard");
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Raised);
    card->setStyleSheet(kCardStyle(isDark_));

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(12,10,12,10);
    v->setSpacing(6);
    auto* name = new QLabel(QString("<b>%1</b>").arg(title), card);
    name->setObjectName("tileTitle");
    name->setTextFormat(Qt::RichText);
    v->addWidget(name);

    // Placeholder: you will plug in your curve editor or live preview here
    v->addWidget(new QLabel("Curve/Trigger/Mix editor (TBD)", card));
    return card;
}

QWidget* MainWindow::makeChannelTile(const QJsonObject& ch) {
    auto* tile = new FanTile(this);
    tile->setTitle(ch.value("name").toString());
    tile->setSensor(ch.value("sensor").toString());
    // duty/temp are fed by onTelemetry()

    connect(tile, &FanTile::editRequested, this, [this, ch](){
        // TODO: open channel editor dialog (not in scope of this patch)
        statusBar()->showMessage(QString("Edit %1 (TBD)").arg(ch.value("name").toString()), 1200);
    });

    // keep a map of labels for telemetry updates
    const QString id = ch.value("id").toString();
    ChannelCardRefs refs;
    refs.card   = tile;
    // We don't have inner QLabel pointers (FanTile encapsulates), but we still keep the QWidget
    // For telemetry, we call its setters via find in chCards_
    chCards_[id] = refs;
    return tile;
}

void MainWindow::applyHideSensors() {
    // rebuild sensor grid, skipping hidden
    for (int i = sensorsGrid_->count() - 1; i >= 0; --i) {
        auto* item = sensorsGrid_->takeAt(i);
        if (auto* w = item->widget()) { w->hide(); w->deleteLater(); }
        delete item;
    }
    sensorCards_.clear();
    int cols = 4, row=0, col=0;
    for (const auto& it : sensorsCache_) {
        const auto s = it.toObject();
        const QString label = s.value("label").toString();
        if (hiddenSensors_.contains(label)) continue;
        auto* w = makeSensorTile(s, /*checked*/true);
        sensorCards_.insert(label, w);
        sensorsGrid_->addWidget(w, row, col);
        if (++col >= cols) { col=0; ++row; }
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
    auto e = rpc_->enumerate();
    if (!e.contains("result")) {
        statusBar()->showMessage("enumerate failed", 1500);
        return;
    }
    const auto r = e["result"].toObject();
    const auto sensors = r.value("sensors").toArray();
    const auto pwms    = r.value("pwms").toArray();

    // Batch createChannel for each PWM
    QJsonArray batch;
    int idCounter = 1;
    for (const auto& it : pwms) {
        const auto p = it.toObject();
        const QString label = p.value("label").toString();
        const QString pwmPath = p.value("pwm").toString();

        const QString sensorPath = chooseSensorForPwm(label, sensors);
        if (sensorPath.isEmpty()) continue;

        QJsonObject params;
        params["name"]   = label;
        params["sensor"] = sensorPath;
        params["pwm"]    = pwmPath;

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
        QTimer::singleShot(200, this, &MainWindow::refresh);
    } else {
        statusBar()->showMessage("No PWM devices to create channels for", 2000);
    }
}

void MainWindow::refresh() {
    // enumerate: sensors + pwms
    auto e = rpc_->enumerate();
    if (e.contains("result")) {
        auto r = e["result"].toObject();
        sensorsCache_ = r.value("sensors").toArray();
        pwmsCache_    = r.value("pwms").toArray();
        rebuildSensors(sensorsCache_);

        // if no channels yet, run detection once
        auto ch = rpc_->listChannels();
        if (ch.isEmpty() && !pwmsCache_.isEmpty()) {
            detect();
            return;
        }
        rebuildChannels(ch);
        rebuildCurves(ch); // placeholder tiles per channel
    } else {
        statusBar()->showMessage("daemon not reachable", 1500);
    }
}

void MainWindow::onTelemetry(const QJsonArray& channels) {
    // Update FanTiles by id
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

void MainWindow::rebuildSensors(const QJsonArray& sensors) {
    // clear grid
    for (int i = sensorsGrid_->count() - 1; i >= 0; --i) {
        auto* item = sensorsGrid_->takeAt(i);
        if (auto* w = item->widget()) { w->hide(); w->deleteLater(); }
        delete item;
    }
    sensorCards_.clear();

    int cols = 4, row=0, col=0;
    for (const auto& it : sensors) {
        const auto s = it.toObject();
        const QString label = s.value("label").toString();
        const bool checked = !hiddenSensors_.contains(label);
        auto* w = makeSensorTile(s, checked);
        sensorCards_.insert(label, w);
        if (checked) {
            sensorsGrid_->addWidget(w, row, col);
            if (++col >= cols) { col=0; ++row; }
        }
    }
}

void MainWindow::rebuildChannels(const QJsonArray& channels) {
    channelsList_->clear();
    chCards_.clear();
    chItems_.clear();

    for (const auto& it : channels) {
        auto obj = it.toObject();
        const QString id = obj.value("id").toString();

        QWidget* tile = makeChannelTile(obj);

        auto* item = new QListWidgetItem();
        item->setSizeHint(QSize(340, 130));
        item->setData(Qt::UserRole, id);
        channelsList_->addItem(item);
        channelsList_->setItemWidget(item, tile);
        chItems_.insert(id, item);
    }
}

void MainWindow::rebuildCurves(const QJsonArray& channels) {
    // clear grid
    for (int i = curvesGrid_->count() - 1; i >= 0; --i) {
        auto* item = curvesGrid_->takeAt(i);
        if (auto* w = item->widget()) { w->hide(); w->deleteLater(); }
        delete item;
    }

    int cols = 2, row=0, col=0;
    for (const auto& it : channels) {
        const auto ch = it.toObject();
        const QString title = ch.value("name").toString() + " — Curve";
        auto* w = makeCurveTile(title);
        curvesGrid_->addWidget(w, row, col);
        if (++col >= cols) { col=0; ++row; }
    }
}
