// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Parcelable;
import java.util.*;

/** Do not use this application's permissions to open arbitrary URIs supplied by another app. */
public final class IncomingShare {
    public static boolean allowed(Context context, Intent intent) {
        try {
            if (!intent.hasExtra(Intent.EXTRA_STREAM))
                return true;
            if (!Intent.ACTION_SEND.equals(intent.getAction())
                && !Intent.ACTION_SEND_MULTIPLE.equals(intent.getAction()))
                return false;
            if ((intent.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION) == 0)
                return false;
            ClipData clip = intent.getClipData();
            if (clip == null)
                return false;
            List<Parcelable> streams = new ArrayList<>();
            if (Intent.ACTION_SEND_MULTIPLE.equals(intent.getAction())) {
                ArrayList<Parcelable> values = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                if (values != null)
                    streams.addAll(values);
            } else {
                Parcelable value = intent.getParcelableExtra(Intent.EXTRA_STREAM);
                if (value != null)
                    streams.add(value);
            }
            for (Parcelable value : streams) {
                if (!(value instanceof Uri))
                    return false;
                Uri uri = (Uri) value;
                if (!"content".equals(uri.getScheme()) || uri.getAuthority() == null)
                    return false;
                String authority = uri.getAuthority().toLowerCase(Locale.ROOT);
                String own = context.getPackageName().toLowerCase(Locale.ROOT);
                if (authority.equals(own) || authority.startsWith(own + "."))
                    return false;
                boolean granted = false;
                for (int index = 0; index < clip.getItemCount(); index++)
                    if (uri.equals(clip.getItemAt(index).getUri())) {
                        granted = true;
                        break;
                    }
                if (!granted)
                    return false;
            }
            return !streams.isEmpty();
        } catch (RuntimeException invalid) {
            return false;
        }
    }
    private IncomingShare() {}
}
