// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import static android.accessibilityservice.AccessibilityService.*;

import android.accessibilityservice.*;
import android.app.KeyguardManager;
import android.graphics.*;
import android.hardware.HardwareBuffer;
import android.os.Bundle;
import android.util.Base64;
import android.view.Display;
import android.view.WindowManager;
import android.view.accessibility.*;
import java.io.ByteArrayOutputStream;
import org.json.*;

final class PhoneAccessibility {
    private final AccessibilityService service;
    private PhoneAccessibility(AccessibilityService value) {
        service = value;
    }
    interface Result {
        void complete(boolean ok, String error, JSONObject value);
    }
    static void perform(JSONObject command, Result done) {
        AccessibilityService bridge =
            org.eternalself.umbrielremote.engine.KdeConnectAccessibilityService.Companion.getInstance();
        if (bridge == null) {
            done.complete(
                false, "Activa el permiso de accesibilidad de Umbriel Connect en el teléfono", null);
            return;
        }
        if (bridge.getSystemService(KeyguardManager.class).isDeviceLocked()) {
            done.complete(false, "El teléfono está bloqueado", null);
            return;
        }
        try {
            new PhoneAccessibility(bridge).execute(command, done);
        } catch (Exception e) {
            done.complete(false, "Acción o argumentos no válidos", null);
        }
    }
    private float coordinate(JSONArray args, int index, int extent) throws JSONException {
        double value = args.getDouble(index);
        if (!Double.isFinite(value) || value < 0 || value > 1)
            throw new JSONException("Coordenada fuera de rango");
        return (float) (value * Math.max(0, extent - 1));
    }
    private void execute(JSONObject command, Result done) throws Exception {
        String action = command.getString("action");
        JSONArray args = command.optJSONArray("args");
        if (args == null)
            args = new JSONArray();
        int global = action.equals("back") ? GLOBAL_ACTION_BACK
            : action.equals("home")        ? GLOBAL_ACTION_HOME
            : action.equals("recents")     ? GLOBAL_ACTION_RECENTS
                                           : 0;
        if (global != 0) {
            if (args.length() != 0)
                throw new JSONException("Sin argumentos");
            boolean ok = service.performGlobalAction(global);
            done.complete(ok, ok ? "" : "Android rechazó la acción", null);
            return;
        }
        if (action.equals("text")) {
            if (args.length() != 1)
                throw new JSONException("Texto requerido");
            String text = args.getString(0);
            if (text.length() > 2000)
                throw new JSONException("Texto demasiado largo");
            AccessibilityNodeInfo root = service.getRootInActiveWindow();
            AccessibilityNodeInfo field =
                root == null ? null : root.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
            if (field == null || !field.isEditable() || field.isPassword()) {
                done.complete(false, "Selecciona un campo de texto que no sea una contraseña", null);
                return;
            }
            Bundle bundle = new Bundle();
            bundle.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, text);
            boolean ok = field.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, bundle);
            done.complete(ok, ok ? "" : "No se pudo escribir en ese campo", null);
            return;
        }
        if (action.equals("tap") || action.equals("swipe")) {
            if (args.length() != (action.equals("tap") ? 2 : 4))
                throw new JSONException("Coordenadas requeridas");
            Rect bounds = service.getSystemService(WindowManager.class).getCurrentWindowMetrics().getBounds();
            Path path = new Path();
            path.moveTo(coordinate(args, 0, bounds.width()), coordinate(args, 1, bounds.height()));
            if (action.equals("swipe"))
                path.lineTo(coordinate(args, 2, bounds.width()), coordinate(args, 3, bounds.height()));
            GestureDescription gesture = new GestureDescription.Builder()
                                             .addStroke(new GestureDescription.StrokeDescription(
                                                 path, 0, action.equals("tap") ? 70 : 350))
                                             .build();
            boolean queued = service.dispatchGesture(gesture, new GestureResultCallback() {
                @Override
                public void onCompleted(GestureDescription g) {
                    done.complete(true, "", null);
                }
                @Override
                public void onCancelled(GestureDescription g) {
                    done.complete(false, "Gesto cancelado por Android", null);
                }
            }, null);
            if (!queued)
                done.complete(false, "Android no aceptó el gesto", null);
            return;
        }
        if (action.equals("screen")) {
            if (args.length() != 0)
                throw new JSONException("Sin argumentos");
            service.takeScreenshot(
                Display.DEFAULT_DISPLAY, service.getMainExecutor(), new TakeScreenshotCallback() {
                    @Override
                    public void onSuccess(ScreenshotResult result) {
                        HardwareBuffer buffer = result.getHardwareBuffer();
                        Bitmap hardware = null, copy = null, scaled = null;
                        try {
                            // A capture requested before revocation must not leave the phone after
                            // revocation.
                            RemoteService remote = RemoteService.instance;
                            if (remote == null || !remote.isAuthorized() || !remote.phoneControlAllowed()
                                || service.getSystemService(KeyguardManager.class).isDeviceLocked()) {
                                done.complete(false, "Sesión revocada", null);
                                return;
                            }
                            hardware = Bitmap.wrapHardwareBuffer(buffer, result.getColorSpace());
                            if (hardware == null)
                                throw new IllegalStateException();
                            copy = hardware.copy(Bitmap.Config.ARGB_8888, false);
                            double scale =
                                Math.min(1.0, 1024.0 / Math.max(copy.getWidth(), copy.getHeight()));
                            scaled =
                                Bitmap.createScaledBitmap(copy, Math.max(1, (int) (copy.getWidth() * scale)),
                                    Math.max(1, (int) (copy.getHeight() * scale)), true);
                            ByteArrayOutputStream out = new ByteArrayOutputStream();
                            scaled.compress(Bitmap.CompressFormat.PNG, 100, out);
                            if (out.size() > 900000)
                                throw new IllegalStateException("Imagen demasiado grande");
                            done.complete(true, "",
                                new JSONObject()
                                    .put("png", Base64.encodeToString(out.toByteArray(), Base64.NO_WRAP))
                                    .put("width", scaled.getWidth())
                                    .put("height", scaled.getHeight()));
                        } catch (Exception e) {
                            done.complete(false, "No se pudo procesar la captura", null);
                        } finally {
                            if (scaled != null && scaled != copy)
                                scaled.recycle();
                            if (copy != null)
                                copy.recycle();
                            if (hardware != null)
                                hardware.recycle();
                            buffer.close();
                        }
                    }
                    @Override
                    public void onFailure(int code) {
                        done.complete(false,
                            "Android rechazó la captura (" + code
                                + "). Las ventanas protegidas permanecen privadas.",
                            null);
                    }
                });
            return;
        }
        done.complete(false, "Acción de teléfono desconocida", null);
    }
}
