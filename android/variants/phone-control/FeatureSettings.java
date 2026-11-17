// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.widget.LinearLayout;
import java.util.function.Consumer;
final class FeatureSettings {
    static void build(MainActivity a,UiDesign ui,LinearLayout target,Consumer<String> status){
        ui.add(target,ui.label("Esta edición controla tu computadora. La edición completa incluye sincronización y funciones del teléfono.",15,ui.muted,false),12);
    }
}
