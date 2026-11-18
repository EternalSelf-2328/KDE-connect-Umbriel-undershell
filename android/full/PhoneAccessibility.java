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
                false, Messages.t("Activa el permiso de accesibilidad de Umbriel Connect en el teléfono"), null);
            return;
        }
        if (bridge.getSystemService(KeyguardManager.class).isDeviceLocked()) {
            done.complete(false, Messages.t("El teléfono está bloqueado"), null);
            return;
        }
        try {
            new PhoneAccessibility(bridge).execute(command, done);
        } catch (Exception e) {
            done.complete(false, Messages.t("Acción o argumentos no válidos"), null);
        }
    }
    private float coordinate(JSONArray args, int index, int extent) throws JSONException {
        double value = args.getDouble(index);
        if (!Double.isFinite(value) || value < 0 || value > 1)
            throw new JSONException(Messages.t("Coordenada fuera de rango"));
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
                throw new JSONException(Messages.t("Sin argumentos"));
            boolean ok = service.performGlobalAction(global);
            done.complete(ok, ok ? "" : Messages.t("Android rechazó la acción"), null);
            return;
        }
        if (action.equals("text")||action.equals("type")||action.equals("key")) {
            if (args.length() != 1)
                throw new JSONException(Messages.t("Texto requerido"));
            String text = args.getString(0);
            if (text.length() > 2000)
                throw new JSONException(Messages.t("Texto demasiado largo"));
            AccessibilityNodeInfo root = service.getRootInActiveWindow();
            AccessibilityNodeInfo field =
                root == null ? null : root.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
            if (field == null || !field.isEditable() || field.isPassword()) {
                done.complete(false, Messages.t("Selecciona un campo de texto que no sea una contraseña"), null);
                return;
            }
            if(action.equals("key")&&!java.util.Arrays.asList("enter","tab","backspace","delete","left","right","home","end").contains(text))throw new JSONException(Messages.t("Tecla no válida"));
            String original=field.getText()==null?"":field.getText().toString();if(original.length()>20000)throw new JSONException(Messages.t("Campo demasiado largo"));int start=field.getTextSelectionStart(),end=field.getTextSelectionEnd();if(start<0||end<start||end>original.length())start=end=original.length();
            int caret=-1;
            if(action.equals("key")){
                if(text.equals("enter")){boolean entered=field.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_IME_ENTER.getId());done.complete(entered,entered?"":Messages.t("Ese campo no permite Intro"),null);return;}
                if(text.equals("tab")){AccessibilityNodeInfo next=field.focusSearch(android.view.View.FOCUS_FORWARD);boolean focused=next!=null&&next.performAction(AccessibilityNodeInfo.ACTION_FOCUS);done.complete(focused,focused?"":Messages.t("No hay otro campo disponible"),null);return;}
                if(text.equals("backspace")){if(start==end&&start>0)start=original.offsetByCodePoints(start,-1);text="";}else if(text.equals("delete")){if(start==end&&end<original.length())end=original.offsetByCodePoints(end,1);text="";}else{caret=text.equals("home")?0:text.equals("end")?original.length():text.equals("left")?(start>0?original.offsetByCodePoints(start,-1):0):(end<original.length()?original.offsetByCodePoints(end,1):original.length());Bundle select=new Bundle();select.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT,caret);select.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT,caret);boolean moved=field.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION,select);done.complete(moved,moved?"":Messages.t("No se pudo mover el cursor"),null);return;}
            }
            if(!action.equals("text")){caret=start+text.length();text=original.substring(0,start)+text+original.substring(end);}
            Bundle bundle = new Bundle();
            bundle.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, text);
            boolean ok = field.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, bundle);
            if(ok&&caret>=0){Bundle select=new Bundle();select.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT,caret);select.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT,caret);field.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION,select);}
            done.complete(ok, ok ? "" : Messages.t("No se pudo escribir en ese campo"), null);
            return;
        }
        if (action.equals("tap") || action.equals("swipe")) {
            if (args.length() != (action.equals("tap") ? 2 : 4))
                throw new JSONException(Messages.t("Coordenadas requeridas"));
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
                    done.complete(false, Messages.t("Gesto cancelado por Android"), null);
                }
            }, null);
            if (!queued)
                done.complete(false, Messages.t("Android no aceptó el gesto"), null);
            return;
        }
        if (action.equals("screen")) {
            if (args.length() != 0)
                throw new JSONException(Messages.t("Sin argumentos"));
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
                                done.complete(false, Messages.t("Sesión revocada"), null);
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
                                throw new IllegalStateException(Messages.t("Imagen demasiado grande"));
                            done.complete(true, "",
                                new JSONObject()
                                    .put("png", Base64.encodeToString(out.toByteArray(), Base64.NO_WRAP))
                                    .put("width", scaled.getWidth())
                                    .put("height", scaled.getHeight()));
                        } catch (Exception e) {
                            done.complete(false, Messages.t("No se pudo procesar la captura"), null);
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
                            Messages.t("Android rechazó la captura (") + code
                                + Messages.t("). Las ventanas protegidas permanecen privadas."),
                            null);
                    }
                });
            return;
        }
        done.complete(false, Messages.t("Acción de teléfono desconocida"), null);
    }
}
