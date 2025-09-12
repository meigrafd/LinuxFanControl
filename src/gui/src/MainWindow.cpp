#include "MainWindow.h"
#include "RpcClient.h"
#include "TelemetryWorker.h"

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
#include <QTimer>
#include <QMetaObject>
#include <QSpacerItem>
#include <QCheckBox>
#include <QPalette>

static QString cardStyle(bool dark) {
    // subtle shadow-look via border + bg
    if (dark) {
        return QStringLiteral(
            "QFrame {"
            "  background:#2f2f2f;"
            "  border:1px solid #444;"
            "  border-radius:12px;"
            "}"
            "QLabel { color:#e8e8e8; }");
    } else {
        return QStringLiteral(
            "QFrame {"
            "  background:#ffffff;"
            "  border:1px solid #d0d0d0;"
            "  border-radius:12px;"
            "}"
            "QLabel { color:#222; }");
    }
}

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent) {
    rpc_ = new RpcClient();
    tw_  = new TelemetryWorker(rpc_, this);

    buildUi();
    resize(1280, 900);

    // initial refresh after UI is up
    QTimer::singleShot(120, this, &MainWindow::refresh);

    // telemetry feed
    connect(tw_, &TelemetryWorker::tickReady, this, &MainWindow::onTelemetry);
    tw_->start(1000);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    auto* tb = addToolBar("toolbar");

    actDetect_  = new QAction("Detect", this);
    actRefresh_ = new QAction("Refresh", this);
    actStart_   = new QAction("Start", this);
    actStop_    = new QAction("Stop", this);
    actTheme_   = new QAction("Light Mode", this); // starts dark=false => offer Light

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

    // TOP (Channels / Fans)
    saTop_ = new QScrollArea(this);
    saTop_->setWidgetResizable(true);
    wrapTop_ = new QWidget(this);
    gridTop_ = new QGridLayout(wrapTop_);
    gridTop_->setContentsMargins(12,12,12,12);
    gridTop_->setHorizontalSpacing(12);
    gridTop_->setVerticalSpacing(12);
    saTop_->setWidget(wrapTop_);
    splitter_->addWidget(saTop_);

    // BOTTOM (Sensors overview with hide-apply)
    saBottom_ = new QScrollArea(this);
    saBottom_->setWidgetResizable(true);
    wrapBottom_ = new QWidget(this);
    auto* vb = new QVBoxLayout(wrapBottom_);
    btnApplyHide_ = new QPushButton("Apply Hide", wrapBottom_);
    btnApplyHide_->setToolTip("Hide all unchecked sensors from the overview");
    connect(btnApplyHide_, &QPushButton::clicked, this, &MainWindow::applyHideSensors);
    vb->addWidget(btnApplyHide_, 0, Qt::AlignLeft);

    auto* cont = new QWidget(wrapBottom_);
    gridBottom_ = new QGridLayout(cont);
    gridBottom_->setContentsMargins(12,12,12,12);
    gridBottom_->setHorizontalSpacing(12);
    gridBottom_->setVerticalSpacing(12);
    vb->addWidget(cont);

    saBottom_->setWidget(wrapBottom_);
    splitter_->addWidget(saBottom_);

    statusBar()->showMessage("Ready", 1500);
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

// ---------------- Theme ----------------

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

    // restyle cards
    for (auto& ref : chCards_) {
        if (ref.card) ref.card->setStyleSheet(cardStyle(isDark_));
    }
    for (auto it = sensorCards_.begin(); it != sensorCards_.end(); ++it) {
        if (it.value()) it.value()->setStyleSheet(cardStyle(isDark_));
    }
}

// --------------- Sensor / Channel cards ----------------

QWidget* MainWindow::makeSensorCard(const QJsonObject& s, bool checked) {
    auto* card = new QFrame;
    card->setObjectName("sensorCard");
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Raised);
    card->setStyleSheet(cardStyle(isDark_));

    auto* v = new QVBoxLayout(card);
    const QString label = s.value("label").toString();
    const QString path  = s.value("path").toString();
    const QString type  = s.value("type").toString("Unknown");

    auto* top = new QHBoxLayout();
    auto* chk = new QCheckBox(card);
    chk->setChecked(checked);
    chk->setToolTip("Check to keep this sensor visible. Click 'Apply Hide' to hide unchecked.");
    top->addWidget(chk);
    auto* name = new QLabel(QString("<b>%1</b>").arg(label), card);
    name->setTextFormat(Qt::RichText);
    top->addWidget(name, 1);
    v->addLayout(top);

    v->addWidget(new QLabel(QString("type: %1").arg(type), card));
    v->addWidget(new QLabel(QString("path: %1").arg(path), card));

    // Track state through widget property (label as key)
    card->setProperty("sensorLabel", label);
    chk->setProperty("sensorLabel", label);
    connect(chk, &QCheckBox::toggled, this, [this, label](bool on){
        if (on) hiddenSensors_.remove(label);
        else    hiddenSensors_.insert(label);
    });
        return card;
}

QWidget* MainWindow::makeChannelCard(const QJsonObject& ch) {
    auto* card = new QFrame;
    card->setObjectName("channelCard");
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Raised);
    card->setStyleSheet(cardStyle(isDark_));

    auto* v = new QVBoxLayout(card);

    auto* name = new QLabel(QString("<b>%1</b>").arg(ch.value("name").toString()), card);
    name->setTextFormat(Qt::RichText);

    auto* sensor = new QLabel(QString("sensor: %1").arg(ch.value("sensor").toString()), card);
    auto* duty   = new QLabel(QString("duty: -- %"), card);
    auto* temp   = new QLabel(QString("temp: -- °C"), card);

    v->addWidget(name);
    v->addWidget(sensor);
    v->addWidget(duty);
    v->addWidget(temp);

    // store refs
    const QString id = ch.value("id").toString();
    chCards_[id] = ChannelCardRefs{card, name, sensor, duty, temp};
    return card;
}

// --------------- Heuristic sensor choice for PWM ----------------

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

            // fallback: first sensor
            if (!sensors.isEmpty()) return sensors.first().toObject().value("path").toString();
            return {};
}

// ---------------------- Actions ------------------------

void MainWindow::applyHideSensors() {
    // rebuild bottom grid, skipping hidden labels
    clearGrid(gridBottom_);
    int cols = 4, row=0, col=0;
    sensorCards_.clear();

    for (const auto& it : sensorsCache_) {
        const auto s = it.toObject();
        const QString label = s.value("label").toString();
        if (hiddenSensors_.contains(label)) continue;
        auto* w = makeSensorCard(s, /*checked*/true);
        sensorCards_.insert(label, w);
        gridBottom_->addWidget(w, row, col);
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

void MainWindow::detect() {
    // Pull enumerate
    auto e = rpc_->enumerate();
    if (!e.contains("result")) {
        statusBar()->showMessage("enumerate failed", 1500);
        return;
    }
    const auto r = e["result"].toObject();
    const auto sensors = r.value("sensors").toArray();
    const auto pwms    = r.value("pwms").toArray();

    // Build a batch: createChannel for each PWM with a chosen sensor
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
    // Enumerate for sensors + pwms list
    auto e = rpc_->enumerate();
    if (e.contains("result")) {
        auto r = e["result"].toObject();
        sensorsCache_ = r.value("sensors").toArray();
        pwmsCache_    = r.value("pwms").toArray();
        rebuildSensors(sensorsCache_);

        // If no channels exist yet, offer detection automatically
        auto ch = rpc_->listChannels();
        if (ch.isEmpty() && !pwmsCache_.isEmpty()) {
            // auto-detect once
            detect();
            return;
        }
        rebuildChannels(ch);
    } else {
        statusBar()->showMessage("daemon not reachable", 1500);
    }
}

void MainWindow::onTelemetry(const QJsonArray& channels) {
    // Update existing cards with duty/temp
    for (const auto& it : channels) {
        const auto ch = it.toObject();
        const QString id   = ch.value("id").toString();
        const double duty  = ch.value("manual").toDouble(); // read mode-agnostic? fallback to last_out?
        const double lastOut  = ch.contains("last_out")  ? ch["last_out"].toDouble()  : NAN;
        const double lastTemp = ch.contains("last_temp") ? ch["last_temp"].toDouble() : NAN;

        auto refsIt = chCards_.find(id);
        if (refsIt == chCards_.end()) continue;
        auto& refs = refsIt.value();

        if (std::isfinite(lastOut))
            refs.duty->setText(QString("duty: %1 %").arg(QString::number(lastOut, 'f', 0)));
        else
            refs.duty->setText(QString("duty: %1 %").arg(QString::number(duty, 'f', 0)));

        if (std::isfinite(lastTemp))
            refs.temp->setText(QString("temp: %1 °C").arg(QString::number(lastTemp, 'f', 1)));
        else
            refs.temp->setText(QString("temp: -- °C"));
    }
}

// --------------- Rebuild sections ----------------------

void MainWindow::rebuildSensors(const QJsonArray& sensors) {
    clearGrid(gridBottom_);
    sensorCards_.clear();

    int cols = 4, row=0, col=0;
    for (const auto& it : sensors) {
        const auto s = it.toObject();
        const QString label = s.value("label").toString();
        const bool checked = !hiddenSensors_.contains(label);
        auto* w = makeSensorCard(s, checked);
        sensorCards_.insert(label, w);
        if (checked) {
            gridBottom_->addWidget(w, row, col);
            if (++col >= cols) { col=0; ++row; }
        } // unchecked remain not added; press "Apply Hide" to rebuild quickly
    }
}

void MainWindow::rebuildChannels(const QJsonArray& channels) {
    clearGrid(gridTop_);
    chCards_.clear();

    int cols = 3, row=0, col=0;
    for (const auto& it : channels) {
        auto obj = it.toObject();
        auto* w = makeChannelCard(obj);
        gridTop_->addWidget(w, row, col);
        if (++col >= cols) { col=0; ++row; }
    }
}
