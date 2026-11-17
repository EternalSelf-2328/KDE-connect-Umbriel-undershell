// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;

import android.app.Application;
import android.content.*;
import android.net.Uri;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
@RunWith(RobolectricTestRunner.class)
@Config(application = Application.class, sdk = 36)
public class IncomingShareTest {
    private Context context;
    private final Uri document = Uri.parse("content://example.documents/document/42");
    @Before
    public void setup() {
        context = RuntimeEnvironment.getApplication();
    }
    private Intent share(Uri uri) {
        Intent value = new Intent(Intent.ACTION_SEND).putExtra(Intent.EXTRA_STREAM, uri);
        value.setClipData(ClipData.newRawUri("Document", uri));
        value.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        return value;
    }

    @Test
    public void grantedDocumentAccepted() {
        assertTrue(IncomingShare.allowed(context, share(document)));
    }
    @Test
    public void missingGrantRejected() {
        Intent i = share(document);
        i.setFlags(0);
        assertFalse(IncomingShare.allowed(context, i));
    }
    @Test
    public void missingClipRejected() {
        Intent i = share(document);
        i.setClipData(null);
        assertFalse(IncomingShare.allowed(context, i));
    }
    @Test
    public void mismatchedClipRejected() {
        Intent i = share(document);
        i.setClipData(ClipData.newRawUri("Other", Uri.parse("content://example.documents/other")));
        assertFalse(IncomingShare.allowed(context, i));
    }
    @Test
    public void privateFileRejected() {
        assertFalse(IncomingShare.allowed(context,
            share(
                Uri.parse("file:///data/data/" + context.getPackageName() + "/shared_prefs/identity.xml"))));
    }
    @Test
    public void ownProviderRejected() {
        assertFalse(IncomingShare.allowed(context,
            share(
                Uri.parse("content://" + context.getPackageName() + ".fileprovider/private/identity.xml"))));
    }
    @Test
    public void wrongActionRejected() {
        Intent i = share(document);
        i.setAction(Intent.ACTION_VIEW);
        assertFalse(IncomingShare.allowed(context, i));
    }
    @Test
    public void textWithoutStreamAccepted() {
        assertTrue(IncomingShare.allowed(
            context, new Intent(Intent.ACTION_SEND).putExtra(Intent.EXTRA_TEXT, "Texto de ejemplo")));
    }
}
