// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
/** Presentation only. This enum never grants a session or changes a trust decision. */
final class ConnectionUi {
    enum Phase { SEARCHING_LOCAL, REMOTE_REQUIRED, AUTHENTICATING, CONNECTED_LOCAL, CONNECTED_REMOTE, FILE_ONLY, PAUSED }
    private ConnectionUi(){}
}
