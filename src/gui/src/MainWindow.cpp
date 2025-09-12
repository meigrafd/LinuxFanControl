/*
 * Linux Fan Control (LFC)
 * (c) 2025 meigrafd & contributors - MIT License
 */

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
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QPainter>

// --- Theme loader (from .qss files) ---
static QString readTextFile(const QString& rel) {
    const QString base = QCoreApplication::applicationDirPath();
    QFile f(base + "/" + rel);
    if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}
static void applyThemeFile(const QString& relQss) {
    qApp->setStyleSheet(readTextFile(relQss));
}

// --- Simple animated logo: rotate a fan-glyph via timer (no external binary needed) ---
class RotLogo : public QWidget {
    Q_OBJECT
public:
    explicit RotLogo(QWidget* p=nullptr) : QWidget(p) {
        setFixedSize(28, 28);
        t_.setInterval(30);
        connect(&t_, &QTimer::timeout, this, [this]{ ang_ = (ang_ + 6) % 360; update(); });
        t_.start();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter g(this);
        g.setRenderHint(QPainter::Antialiasing, true);
        g.translate(width()/2.0, height()/2.0);
        g.rotate(ang_);
        g.setPen(Qt::NoPen);
        g.setBrush(QColor("#3a7bd5"));
        for (int i=0;i<3;++i) {
            g.save();
            g.rotate(120*i);
            g.drawRoundedRect(QRectF(2,-4,10,8), 3, 3);
            g.restore();
        }
        g.setBrush(QColor("#1b3a5b"));
        g.drawEllipse(QPointF(0,0), 3.5, 3.5);
    }
private:
    QTimer t_;
    int ang_{0};
};

// ASCII-ish prefixes for tiles
static QString fanPrefix()  { return "[FAN] ";  }
static QString tempPrefix() { return "[TEMP] "; }
static QString autoPrefix() { return "[AUTO] "; }
static QString manPrefix()  { return "[MAN] ";  }

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    rpc_ = new RpcClient();
    shm_ = new ShmSubscriber(this);

    auto* tb = addToolBar("toolbar");
    tb->setMovable(false);
    tb->addWidget(new RotLogo(tb)); // animated logo

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

    // Apply default theme from file
    applyThemeFile("assets/themes/dark.qss");
    isDark_ = true;
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
    applyThemeFile(isDark_ ? "assets/themes/dark.qss" : "assets/themes/light.qss");
    actTheme_->setText(isDark_ ? "Light Mode" : "Dark Mode");
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
    const QString nm = ch.value("name").toString();
    const QString sn = ch.value("sensor").toString();
    tile->setTitle(fanPrefix() + nm);
    tile->setSensor(tempPrefix() + sn);
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

#include "MainWindow.moc"
