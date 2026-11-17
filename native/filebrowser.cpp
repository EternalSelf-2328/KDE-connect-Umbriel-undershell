// SPDX-License-Identifier: GPL-3.0-or-later
#include "design.h"
#include <KDirOperator>
#include <KFileItem>
#include <KIO/CopyJob>
#include <KProtocolInfo>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>

class FileWindow : public QDialog {
  public:
    QList<QPointer<KJob>> transfers;
    ~FileWindow() override {
        for (auto job : transfers)
            if (job)
                job->kill();
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("umbriel-connect-files");
    app.setDesktopFileName("org.eternalself.UmbrielConnect.Files");
    Design::install(&app);
    QIcon::setThemeName(Design::palette().background.lightness() < 128 ? "breeze-dark" : "breeze");
    const auto args = app.arguments().mid(1);
    if (args == QStringList{"--check-protocol"})
        return KProtocolInfo::isKnownProtocol(QString("umbriel-connect")) ? 0 : 1;
    const bool preview = args.size() == 2 && args[0] == "--preview";
    if (!preview &&
        (args.size() != 1 || !QRegularExpression("^[A-Za-z0-9_]{1,128}$").match(args[0]).hasMatch()))
        return 2;
    QTemporaryDir demo;
    QUrl root;
    if (preview) {
        if (!demo.isValid())
            return 1;
        QFile note(demo.path() + "/Notas de ejemplo.txt");
        if (!note.open(QIODevice::WriteOnly))
            return 1;
        note.write("Ejemplo de una vista aislada.\n");
        note.close();
        QDir().mkpath(demo.path() + "/Imágenes de ejemplo");
        root = QUrl::fromLocalFile(demo.path() + "/");
    } else {
        root.setScheme("umbriel-connect");
        root.setHost(args[0]);
        root.setPath("/");
    }
    FileWindow window;
    window.setWindowTitle("Archivos · Umbriel Connect");
    const auto screen = app.primaryScreen()->availableGeometry().size();
    window.resize(qMin(900, int(screen.width() * .85)), qMin(680, int(screen.height() * .9)));
    window.setMinimumSize(560, 400);
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(Design::margin, Design::padding, Design::margin, Design::padding);
    layout->setSpacing(Design::gap);
    auto *title = new QLabel("Archivos del teléfono");
    title->setProperty("role", "hero");
    layout->addWidget(title);
    auto *hint = new QLabel(preview ? "Vista previa · documentos sintéticos, sin teléfono conectado."
                                    : "Explora únicamente las carpetas que autorizaste en el teléfono.");
    hint->setProperty("role", "muted");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *row = new QHBoxLayout;
    auto *up = new QPushButton("Subir carpeta");
    auto *refresh = new QPushButton("Actualizar");
    row->addWidget(up);
    row->addWidget(refresh);
    row->addStretch();
    layout->addLayout(row);
    auto *location = new QLineEdit;
    location->setReadOnly(true);
    location->setAccessibleName("Carpeta actual");
    layout->addWidget(location);
    auto *files = new KDirOperator(root, &window);
    files->setMode(KFile::Files | KFile::Directory);
    files->setViewMode(KFile::Detail);
    layout->addWidget(files, 1);
    auto *status = new QLabel("Selecciona un archivo o una carpeta.");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->setProperty("role", "muted");
    layout->addWidget(status);
    auto *progress = new QProgressBar;
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->hide();
    layout->addWidget(progress);
    auto *actions = new QHBoxLayout;
    auto *upload = new QPushButton("Enviar aquí…");
    auto *download = new QPushButton("Descargar…");
    download->setProperty("kind", "primary");
    auto *close = new QPushButton("Cerrar");
    close->setProperty("kind", "ghost");
    actions->addWidget(upload);
    actions->addStretch();
    actions->addWidget(close);
    actions->addWidget(download);
    layout->addLayout(actions);
    QObject::connect(up, &QPushButton::clicked, files, &KDirOperator::cdUp);
    QObject::connect(refresh, &QPushButton::clicked, files, &KDirOperator::rereadDir);
    QObject::connect(close, &QPushButton::clicked, &window, &QDialog::close);
    QObject::connect(files, &KDirOperator::urlEntered, &window, [=](const QUrl &url) {
        location->setText(preview ? "/Teléfono de ejemplo/" : url.toDisplayString(QUrl::RemovePassword));
    });
    location->setText(preview ? "/Teléfono de ejemplo/" : root.toDisplayString());
    auto transfer = [&](const QList<QUrl> &source, const QUrl &destination) {
        auto *job = KIO::copy(source, destination, KIO::HideProgressInfo);
        window.transfers.append(job);
        upload->setEnabled(false);
        download->setEnabled(false);
        progress->setValue(0);
        progress->show();
        status->setText("Transfiriendo…");
        QObject::connect(job, &KJob::percentChanged, &window,
                         [progress](KJob *, unsigned long value) { progress->setValue(value); });
        QObject::connect(job, &KJob::result, &window, [=, &window](KJob *done) {
            status->setText(done->error() ? done->errorText() : "Transferencia terminada.");
            window.transfers.removeIf([done](const QPointer<KJob> &job) { return !job || job == done; });
            progress->hide();
            upload->setEnabled(true);
            download->setEnabled(true);
            files->rereadDir();
        });
    };
    QObject::connect(download, &QPushButton::clicked, &window, [&] {
        const auto selected = files->selectedItems();
        if (selected.isEmpty()) {
            status->setText("Selecciona primero un archivo o una carpeta.");
            return;
        }
        const auto item = selected.first();
        QString target;
        if (selected.size() > 1 || item.isDir())
            target = QFileDialog::getExistingDirectory(
                &window, "Descargar carpeta",
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                QFileDialog::DontUseNativeDialog);
        else
            target = QFileDialog::getSaveFileName(
                &window, "Descargar archivo",
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" +
                    QFileInfo(item.name()).fileName(),
                {}, nullptr, QFileDialog::DontUseNativeDialog);
        if (!target.isEmpty()) {
            QList<QUrl> urls;
            for (const auto &entry : selected)
                urls.append(entry.url());
            transfer(urls, QUrl::fromLocalFile(target));
        }
    });
    QObject::connect(upload, &QPushButton::clicked, &window, [&] {
        const auto paths = QFileDialog::getOpenFileNames(&window, "Enviar al teléfono", {}, {}, nullptr,
                                                       QFileDialog::DontUseNativeDialog);
        if (!paths.isEmpty()) {
            QList<QUrl> urls;
            for (const auto &path : paths) urls.append(QUrl::fromLocalFile(path));
            transfer(urls, files->url());
        }
    });
    QObject::connect(files, &KDirOperator::fileSelected, &window,
                     [download](const KFileItem &) { download->click(); });
    if (preview) {
        for (auto *button : {up, refresh, upload, download})
            button->setEnabled(false);
        QTimer::singleShot(900, &window, [&] { app.exit(window.grab().save(args[1]) ? 0 : 1); });
    }
    window.show();
    return app.exec();
}
