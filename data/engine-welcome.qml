/*
 * SPDX-FileCopyrightText: 2024 Darshan Phaldesai <dev.darshanphaldesai@gmail.org>
 * SPDX-FileCopyrightText: 2026 EternalSelf-2328
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.FormCardPage {
    Kirigami.Icon {
        source: "org.eternalself.UmbrielConnect"
        implicitWidth: Kirigami.Units.iconSizes.huge
        implicitHeight: Kirigami.Units.iconSizes.huge
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.gridUnit
    }

    Kirigami.Heading {
        text: i18nc("@title", "Select your phone")
        horizontalAlignment: Qt.AlignHCenter
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
    }

    Controls.Label {
        text: i18nc("@info", "Pair your phone from the Home page of Umbriel Connect, then select it here to manage its features.")
        horizontalAlignment: Qt.AlignHCenter
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
    }

    FormCard.FormHeader {
        title: i18nc("@title:context", "Umbriel Connect for Android")
        Layout.topMargin: Kirigami.Units.gridUnit
    }

    FormCard.FormCard {
        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Download Android app")
            onClicked: Qt.openUrlExternally("https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell/releases")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18nc("@action:button", "Pairing and connection guide")
            onClicked: Qt.openUrlExternally("https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell/blob/main/docs/LOCAL-FILES.md")
        }
    }
}
