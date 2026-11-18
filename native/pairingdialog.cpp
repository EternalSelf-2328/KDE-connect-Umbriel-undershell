// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "pairingdialog.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QDir>
#include <memory>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#ifdef UMBRIEL_PAIR_QR
#include <Prison/Barcode>
#endif
namespace PairingUi {
QString link(const QJsonObject &registration){QUrl url("umbriel-connect://pair");QUrlQuery query;query.addQueryItem("data",QString::fromLatin1(QJsonDocument(registration).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)));url.setQuery(query);return url.toString(QUrl::FullyEncoded);}
QImage code(const QString &text,int pixels){
#ifdef UMBRIEL_PAIR_QR
    auto qr=Prison::Barcode::create(Prison::QRCode);if(qr){qr->setData(text);return qr->toImage(QSizeF(pixels,pixels));}
#else
    Q_UNUSED(text);Q_UNUSED(pixels);
#endif
    return {};
}
bool save(const QString &path,const QJsonObject &registration){QSaveFile file(path);const auto bytes=QJsonDocument(registration).toJson();return file.open(QIODevice::WriteOnly)&&file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)&&file.write(bytes)==bytes.size()&&file.commit();}
void open(QWidget *parent,const QStringList &hosts,quint16 port,const QString &helper){
    auto *dialog=new QDialog(parent);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(I18n::text("Vincular teléfono"));auto *layout=new QVBoxLayout(dialog);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);auto *body=new QWidget;auto *form=new QVBoxLayout(body);form->setContentsMargins(24,24,24,24);form->setSpacing(12);scroll->setWidget(body);layout->addWidget(scroll);
    auto label=[form](QString text){auto *l=new QLabel(text);l->setTextFormat(Qt::PlainText);l->setWordWrap(true);form->addWidget(l);return l;};
    auto *title=label(I18n::text("Tu computadora, en tu teléfono"));title->setProperty("role","title");label(I18n::text("Genera un vínculo privado. Escanea el QR con la cámara del teléfono y abre Umbriel Connect, o importa el archivo en Inicio → Vincular computadora."));
    auto *addresses=new QComboBox;addresses->addItems(hosts);form->addWidget(addresses);addresses->setVisible(hosts.size()>1);
    auto *generate=new QPushButton(I18n::text("Generar vínculo de 5 minutos"));generate->setProperty("kind","primary");form->addWidget(generate);
    auto *qr=label("");qr->setAlignment(Qt::AlignCenter);auto *status=label(hosts.isEmpty()?I18n::text("Conecta la computadora a una red local y vuelve a abrir esta ventana."):I18n::text("Solo hace falta vincular una vez. Después, la conexión local será automática."));
    auto registration=std::make_shared<QJsonObject>();auto *copy=new QPushButton(I18n::text("Copiar registro"));auto *exportFile=new QPushButton(I18n::text("Guardar archivo…"));form->addWidget(copy);form->addWidget(exportFile);copy->setEnabled(false);exportFile->setEnabled(false);generate->setEnabled(!hosts.isEmpty()&&!helper.isEmpty());
    QObject::connect(generate,&QPushButton::clicked,dialog,[=]{
        generate->setEnabled(false);copy->setEnabled(false);exportFile->setEnabled(false);qr->clear();status->setText(I18n::text("Creando el vínculo privado…"));
        auto *process=new QProcess(dialog);const auto host=addresses->currentText();
        QObject::connect(process,&QProcess::errorOccurred,dialog,[=]{status->setText(I18n::text("No se pudo iniciar el receptor. Abre Umbriel Connect y vuelve a intentarlo."));generate->setEnabled(true);});
        QObject::connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),dialog,[=](int exit,QProcess::ExitStatus){
            generate->setEnabled(true);process->readAllStandardOutput();process->readAllStandardError();
            QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/umbriel-connect/remote/pairing.json");
            if(exit!=0||!file.open(QIODevice::ReadOnly)||file.size()>8192){status->setText(I18n::text("No se pudo generar el registro. Comprueba la conexión local del receptor."));process->deleteLater();return;}
            *registration=QJsonDocument::fromJson(file.readAll()).object();
            if(registration->value("host").toString()!=host||registration->value("port").toInt()!=port||registration->value("expires").toInteger()<=QDateTime::currentSecsSinceEpoch()){status->setText(I18n::text("El registro no corresponde a esta conexión. Vuelve a generarlo."));*registration=QJsonObject{};process->deleteLater();return;}
            const auto image=code(link(*registration),360);if(!image.isNull())qr->setPixmap(QPixmap::fromImage(image));else qr->setText(I18n::text("Guarda el archivo privado y ábrelo desde Inicio en el teléfono."));
            status->setText(I18n::text("Válido hasta ")+QDateTime::fromSecsSinceEpoch(registration->value("expires").toInteger()).toString("HH:mm")+" · se usa una sola vez");copy->setEnabled(true);exportFile->setEnabled(true);
            const auto expiry=registration->value("expires").toInteger();QTimer::singleShot(qMax<qint64>(0,(expiry-QDateTime::currentSecsSinceEpoch())*1000),dialog,[=]{if(registration->value("expires").toInteger()!=expiry)return;qr->clear();copy->setEnabled(false);exportFile->setEnabled(false);*registration=QJsonObject{};status->setText(I18n::text("El vínculo caducó. Genera otro para continuar."));});process->deleteLater();
        });
        process->start(helper,{"pair",host,QString::number(port)});QTimer::singleShot(5000,process,[process]{if(process->state()!=QProcess::NotRunning)process->kill();});
    });
    QObject::connect(copy,&QPushButton::clicked,dialog,[=]{if(registration->isEmpty())return;const auto text=QString::fromUtf8(QJsonDocument(*registration).toJson(QJsonDocument::Compact));QApplication::clipboard()->setText(text);status->setText(I18n::text("Registro privado copiado. En el teléfono usa Ajustes → Mi computadora → Pegar registro."));QTimer::singleShot(300000,qApp,[text]{if(QApplication::clipboard()->text()==text)QApplication::clipboard()->clear();});});
    QObject::connect(exportFile,&QPushButton::clicked,dialog,[=]{if(registration->isEmpty())return;const auto path=QFileDialog::getSaveFileName(dialog,I18n::text("Guardar registro privado"),QDir::homePath()+"/umbriel-pairing.json",I18n::text("Registro (*.json)"));if(!path.isEmpty())status->setText(save(path,*registration)?I18n::text("Registro guardado con permisos privados. Impórtalo en el teléfono."):I18n::text("No se pudo guardar el registro."));});
    const auto available=parent->screen()->availableGeometry().size();dialog->resize(qMin(520,available.width()-48),qMin(680,available.height()-48));dialog->show();
}
}
