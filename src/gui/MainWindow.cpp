#include "MainWindow.h"
#include "FlowLayout.h"
#include "RpcClient.h"
#include "widgets/FanCard.h"
#include "widgets/SensorCard.h"
#include "dialogs/DetectDialog.h"
#include "dialogs/ChannelEditorDialog.h"
#include "TelemetryWorker.h"

#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QSplitter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileDialog>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();
    applyTheme(theme_);
    retranslate();
    startTelemetry();
    refreshAll();
}

MainWindow::~MainWindow() {
    stopTelemetry();
}

void MainWindow::buildUi() {
    auto* tb = addToolBar("tb");

    auto* actTheme = new QAction(this);
    actTheme->setText("Theme");
    connect(actTheme, &QAction::triggered, this, &MainWindow::onToggleTheme);
    tb->addAction(actTheme);

    auto* actImport = new QAction(tr("Import FanControl config…"), this);
    connect(actImport, &QAction::triggered, this, &MainWindow::importFanControlJson);
    tb->addAction(actImport);

    tb->addSeparator();
    comboLang_ = new QComboBox(); comboLang_->addItems({"en","de"});
    comboLang_->setCurrentText(tr_.language());
    connect(comboLang_, &QComboBox::currentTextChanged, this, &MainWindow::onSwitchLang);
    tb->addWidget(new QLabel(t("language")));
    tb->addWidget(comboLang_);

    tb->addSeparator();
    auto* actRefresh = new QAction(t("refresh"), this);
    connect(actRefresh, &QAction::triggered, this, &MainWindow::refreshAll);
    tb->addAction(actRefresh);

    auto* actDetect = new QAction(t("detect"), this);
    connect(actDetect, &QAction::triggered, this, &MainWindow::onOpenDetect);
    tb->addAction(actDetect);

    auto* actStart = new QAction(t("engine_start_short"), this);
    connect(actStart, &QAction::triggered, this, [this]{ onEngine(true); });
    tb->addAction(actStart);

    auto* actStop = new QAction(t("engine_stop_short"), this);
    connect(actStop, &QAction::triggered, this, [this]{ onEngine(false); });
    tb->addAction(actStop);

    splitter_ = new QSplitter(Qt::Orientation::Vertical, this);
    setCentralWidget(splitter_);

    // Top: Channel tiles
    auto* top = new QWidget(); auto* topV = new QVBoxLayout(top);
    topV->addWidget(new QLabel(t("channels")));
    saChannels_ = new QScrollArea(); saChannels_->setWidgetResizable(true);
    wrapChannels_ = new QWidget(); wrapChannels_->setLayout(new FlowLayout());
    saChannels_->setWidget(wrapChannels_);
    topV->addWidget(saChannels_, 1);
    splitter_->addWidget(top);

    // Bottom: Sources with selection & create
    auto* bottom = new QWidget(); auto* bottomV = new QVBoxLayout(bottom);

    auto* row = new QHBoxLayout();
    auto* btnCreate = new QPushButton(t("create_from_selection"));
    connect(btnCreate, &QPushButton::clicked, this, &MainWindow::onCreateFromSelection);
    row->addWidget(btnCreate);
    row->addStretch(1);
    bottomV->addLayout(row);

    auto* grids = new QSplitter(Qt::Orientation::Horizontal);
    // Sensors
    auto* leftWrap = new QWidget(); auto* leftV = new QVBoxLayout(leftWrap);
    leftV->addWidget(new QLabel(t("sensors")));
    saSensors_ = new QScrollArea(); saSensors_->setWidgetResizable(true);
    wrapSensors_ = new QWidget(); wrapSensors_->setLayout(new FlowLayout());
    saSensors_->setWidget(wrapSensors_); leftV->addWidget(saSensors_,1);
    grids->addWidget(leftWrap);
    // PWMs
    auto* rightWrap = new QWidget(); auto* rightV = new QVBoxLayout(rightWrap);
    rightV->addWidget(new QLabel(t("pwms")));
    saPwms_ = new QScrollArea(); saPwms_->setWidgetResizable(true);
    wrapPwms_ = new QWidget(); wrapPwms_->setLayout(new FlowLayout());
    saPwms_->setWidget(wrapPwms_); rightV->addWidget(saPwms_,1);
    grids->addWidget(rightWrap);

    bottomV->addWidget(grids, 1);
    splitter_->addWidget(bottom);

    statusEngine_ = new QLabel("⏹");
    statusBar()->addPermanentWidget(statusEngine_);
}

void MainWindow::applyTheme(const QString& theme) {
    auto* app = QApplication::instance();
    if (theme == "dark") {
        QPalette pal;
        pal.setColor(QPalette::ColorRole::Window, QColor(30,30,30));
        pal.setColor(QPalette::ColorRole::WindowText, Qt::white);
        pal.setColor(QPalette::ColorRole::Base, QColor(45,45,45));
        pal.setColor(QPalette::ColorRole::Text, Qt::white);
        pal.setColor(QPalette::ColorRole::Button, QColor(60,60,60));
        pal.setColor(QPalette::ColorRole::ButtonText, Qt::white);
        pal.setColor(QPalette::ColorRole::Highlight, QColor(80,120,200));
        app->setPalette(pal);
        app->setStyle("Fusion");
    } else {
        app->setPalette(app->style()->standardPalette());
        app->setStyle("Fusion");
    }
}

void MainWindow::retranslate() {
    setWindowTitle("Linux Fan Control");
}

void MainWindow::onToggleTheme() {
    theme_ = (theme_ == "dark") ? "light" : "dark";
    applyTheme(theme_);
}

void MainWindow::onSwitchLang(const QString& code) {
    tr_.setLanguage(code);
    rebuildSources();
    rebuildChannelCards();
}

void MainWindow::rebuildChannelCards() {
    auto* fl = dynamic_cast<FlowLayout*>(wrapChannels_->layout());
    while (fl->count()) {
        auto* it = fl->takeAt(0);
        if (it && it->widget()) it->widget()->deleteLater();
        delete it;
    }
    for (const auto& ch : chans_) {
        FanCard::Model m;
        m.id = ch.id; m.name = ch.name;
        m.sensorPath = ch.sensorPath; m.pwmPath = ch.pwmPath;
        m.enablePath = ch.enablePath; m.mode = ch.mode;
        m.manualPct = ch.manualPct; m.lastTemp = ch.lastTemp; m.lastOut = ch.lastOut;
        auto* card = new FanCard(m);
        card->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(card, &QWidget::customContextMenuRequested, this, [this, card](const QPoint& pos){ onChannelContextMenu(card, pos); });
        connect(card, &FanCard::editRequested, this, &MainWindow::onEditChannel);
        connect(card, &FanCard::modeChanged, this, [this](QString id, QString mode){ rpcSetChannelMode(id, mode); });
        connect(card, &FanCard::manualChanged, this, [this](QString id, double pct){ rpcSetChannelManual(id, pct); });
        fl->addWidget(card);
    }
    wrapChannels_->update();
}

void MainWindow::rebuildSources() {
    // Sensors
    auto* flS = dynamic_cast<FlowLayout*>(wrapSensors_->layout());
    while (flS->count()) {
        auto* it = flS->takeAt(0);
        if (it && it->widget()) it->widget()->deleteLater();
        delete it;
    }
    for (const auto& s : temps_) {
        SensorCard::Model m{ s.label, s.path, s.type };
        auto* card = new SensorCard(m, true);
        connect(card, &SensorCard::toggled, this, [this](QString label, bool on){ selSensors_[label]=on; });
        flS->addWidget(card);
    }
    // PWMs
    auto* flP = dynamic_cast<FlowLayout*>(wrapPwms_->layout());
    while (flP->count()) {
        auto* it = flP->takeAt(0);
        if (it && it->widget()) it->widget()->deleteLater();
        delete it;
    }
    for (const auto& p : pwms_) {
        SensorCard::Model m{ p.label, p.pwmPath, p.writable ? "writable" : "readonly" };
        auto* card = new SensorCard(m, p.writable);
        connect(card, &SensorCard::toggled, this, [this](QString label, bool on){ selPwms_[label]=on; });
        flP->addWidget(card);
    }
    wrapSensors_->update();
    wrapPwms_->update();
}

void MainWindow::startTelemetry() {
    stopTelemetry();
    teleThread_ = new QThread(this);
    tele_ = new TelemetryWorker();
    tele_->moveToThread(teleThread_);
    connect(teleThread_, &QThread::started, tele_, &TelemetryWorker::start);
    connect(teleThread_, &QThread::finished, tele_, &QObject::deleteLater);
    connect(this, &QObject::destroyed, tele_, &TelemetryWorker::stop);
    connect(tele_, &TelemetryWorker::tickReady, this, [this](QJsonArray arr){
        for (auto v : arr) {
            auto o = v.toObject();
            QString id = o["id"].toString();
            auto it = chans_.find(id);
            if (it != chans_.end()) {
                it->lastTemp = o["last_temp"].toDouble();
                it->lastOut  = o["last_out"].toDouble();
            }
        }
        tick();
    });
    teleThread_->start();
}

void MainWindow::stopTelemetry() {
    if (!teleThread_) return;
    QMetaObject::invokeMethod(tele_, "stop", Qt::QueuedConnection);
    teleThread_->quit();
    teleThread_->wait(2000);
    teleThread_ = nullptr; tele_ = nullptr;
}

bool MainWindow::rpcEnumerate() {
    RpcClient cli;
    std::string err;
    auto res = cli.call("enumerate", QJsonObject{}, 15000, &err);
    if (!res) { QMessageBox::warning(this, "LFC", QString::fromStdString(err)); return false; }
    temps_.clear(); pwms_.clear();
    QJsonObject obj = res->toObject();
    for (auto v : obj["temps"].toArray()) {
        auto o = v.toObject();
        temps_.push_back(Temp{ o["label"].toString(), o["path"].toString(), o["type"].toString() });
    }
    for (auto v : obj["pwms"].toArray()) {
        auto o = v.toObject();
        Pwm p;
        p.label = o["label"].toString();
        p.pwmPath = o["pwm_path"].toString();
        p.enablePath = o["enable_path"].toString();
        p.tachPath = o["tach_path"].toString();
        p.writable = o["writable"].toBool();
        pwms_.push_back(p);
    }
    return true;
}

bool MainWindow::rpcListChannels() {
    RpcClient cli; std::string err;
    auto res = cli.call("listChannels", QJsonObject{}, 10000, &err);
    if (!res) { QMessageBox::warning(this, "LFC", QString::fromStdString(err)); return false; }
    chans_.clear();
    for (auto v : res->toArray()) {
        auto o = v.toObject();
        Chan c;
        c.id = o["id"].toString();
        c.name = o["name"].toString();
        c.sensorPath = o["sensor_path"].toString();
        c.pwmPath = o["pwm_path"].toString();
        c.enablePath = o["enable_path"].toString();
        c.mode = o["mode"].toString();
        c.manualPct = o["manual_pct"].toDouble();
        c.lastTemp = o["last_temp"].toDouble();
        c.lastOut = o["last_out"].toDouble();
        chans_.insert(c.id, c);
    }
    return true;
}

bool MainWindow::rpcSetChannelMode(const QString& id, const QString& mode) {
    RpcClient cli; std::string err;
    QJsonObject p{{"id", id}, {"mode", mode}};
    return cli.call("setChannelMode", p, 8000, &err).has_value();
}

bool MainWindow::rpcSetChannelManual(const QString& id, double pct) {
    RpcClient cli; std::string err;
    QJsonObject p{{"id", id}, {"pct", pct}};
    return cli.call("setChannelManual", p, 8000, &err).has_value();
}

bool MainWindow::rpcDeleteChannel(const QString& id) {
    RpcClient cli; std::string err;
    QJsonObject p{{"id", id}};
    return cli.call("deleteChannel", p, 8000, &err).has_value();
}

bool MainWindow::rpcCreateChannel(const QString& name, const QString& sensor, const QString& pwm, const QString& enable) {
    RpcClient cli; std::string err;
    QJsonObject p{{"name", name}, {"sensor_path", sensor}, {"pwm_path", pwm}, {"enable_path", enable},
    {"mode","Auto"}, {"manual_pct",0.0}, {"hyst",0.5}, {"tau",2.0},
    {"curve", QJsonArray{ QJsonArray{20,0}, QJsonArray{35,25}, QJsonArray{50,50}, QJsonArray{70,80} }}};
    return cli.call("createChannel", p, 15000, &err).has_value();
}

void MainWindow::refreshAll() {
    if (!rpcEnumerate()) return;
    rebuildSources();
    refreshChannels();
}

void MainWindow::refreshChannels() {
    if (!rpcListChannels()) return;
    rebuildChannelCards();
}

void MainWindow::tick() {
    auto* fl = dynamic_cast<FlowLayout*>(wrapChannels_->layout());
    for (int i = 0; i < fl->count(); ++i) {
        auto* w = fl->itemAt(i)->widget();
        auto* fc = qobject_cast<FanCard*>(w);
        if (!fc) continue;
        const auto& c = chans_[fc->id()];
        fc->updateTelemetry(c.lastTemp, c.lastOut);
    }
}

void MainWindow::onOpenDetect() {
    DetectDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto res = dlg.result();
        QMessageBox::information(this, "LFC", tr_.t("detection_done", {{"n", res.size()}}));
    }
    refreshChannels();
}

void MainWindow::onCreateFromSelection() {
    QString sensorPath;
    for (const auto& s : temps_) if (selSensors_.value(s.label)) { sensorPath = s.path; break; }
    if (sensorPath.isEmpty()) { QMessageBox::information(this, "LFC", tr_.t("select_sources_first")); return; }
    int created = 0;
    for (const auto& p : pwms_) {
        if (!selPwms_.value(p.label)) continue;
        if (!p.writable) continue;
        if (rpcCreateChannel(p.label, sensorPath, p.pwmPath, p.enablePath)) ++created;
    }
    QMessageBox::information(this, "LFC", tr_.t("created_n", {{"n", created}}));
    refreshChannels();
}

void MainWindow::onEngine(bool start) {
    RpcClient cli; std::string err;
    auto res = cli.call(start ? "engineStart" : "engineStop", QJsonObject{}, 10000, &err);
    if (!res) { QMessageBox::warning(this, "LFC", QString::fromStdString(err)); return; }
    statusEngine_->setText(start ? "▶" : "⏹");
}

void MainWindow::onEditChannel(const QString& id) {
    openChannelEditor(id);
}

void MainWindow::onChannelContextMenu(FanCard* card, const QPoint& pos) {
    QMenu m;
    auto* actRename = m.addAction(tr_.t("rename"));
    auto* actDelete = m.addAction(tr_.t("delete"));
    auto* chosen = m.exec(card->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == actRename) onEditChannel(card->id());
    else if (chosen == actDelete) {
        if (QMessageBox::question(this, "LFC", tr_.t("confirm_delete")) == QMessageBox::StandardButton::Yes) {
            rpcDeleteChannel(card->id());
            refreshChannels();
        }
    }
}

void MainWindow::openChannelEditor(const QString& id) {
    auto it = chans_.find(id); if (it == chans_.end()) return;
    ChannelEditorDialog::Model m;
    m.id = it->id; m.name = it->name;
    m.hyst = 0.5; m.tau = 2.0;
    m.curve = { {20,0},{35,25},{50,50},{70,80} }; // TODO: fetch from daemon if exposed

    ChannelEditorDialog dlg(m, this);
    connect(&dlg, &ChannelEditorDialog::saveRequested, this, [this](QString cid, QString newName, QVector<QPointF> curve, double hyst, double tau){
        rpcSetChannelCurve(cid, curve);
        rpcSetChannelHystTau(cid, hyst, tau);
        auto it = chans_.find(cid); if (it!=chans_.end()) it->name = newName;
        rebuildChannelCards();
    });
    dlg.exec();
}

bool MainWindow::rpcSetChannelCurve(const QString& id, const QVector<QPointF>& pts) {
    QJsonArray arr;
    for (auto& p : pts) arr.push_back(QJsonArray{ p.x(), p.y() });
    RpcClient cli; std::string err;
    return cli.call("setChannelCurve", QJsonObject{{"id",id},{"curve",arr}}, 10000, &err).has_value();
}

bool MainWindow::rpcSetChannelHystTau(const QString& id, double hyst, double tau) {
    RpcClient cli; std::string err;
    return cli.call("setChannelHystTau", QJsonObject{{"id",id},{"hyst",hyst},{"tau",tau}}, 10000, &err).has_value();
}

void MainWindow::importFanControlJson() {
    QString path = QFileDialog::getOpenFileName(this, "Import FanControl JSON", QString(), "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "LFC", "Failed to open file."); return; }
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) { QMessageBox::warning(this, "LFC", "Invalid JSON."); return; }
    auto root = doc.object();

    int created = 0;
    QString defaultSensor;
    if (!temps_.isEmpty()) defaultSensor = temps_.first().path;

    auto tryCreate = [&](const QString& name){
        auto it = std::find_if(pwms_.begin(), pwms_.end(), [&](const Pwm& p){ return name.contains(p.label, Qt::CaseInsensitive); });
        if (it == pwms_.end()) return;
        if (rpcCreateChannel(it->label, defaultSensor, it->pwmPath, it->enablePath)) ++created;
    };
        if (root.contains("Controls") && root["Controls"].isArray()) {
            for (auto v : root["Controls"].toArray()) {
                auto o = v.toObject();
                QString nm = o["Name"].toString();
                if (!nm.isEmpty()) tryCreate(nm);
            }
        } else if (root.contains("MixedCurves") && root["MixedCurves"].isArray()) {
            for (auto v : root["MixedCurves"].toArray()) {
                auto o = v.toObject();
                QString nm = o["Name"].toString();
                if (!nm.isEmpty()) tryCreate(nm);
            }
        }
        QMessageBox::information(this, "LFC", tr_.t("created_n", {{"n", created}}));
        refreshChannels();
}
