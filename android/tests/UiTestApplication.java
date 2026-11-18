package org.eternalself.umbrielremote;
public class UiTestApplication extends android.app.Application {@Override public void onCreate(){super.onCreate();getSharedPreferences("appearance",0).edit().clear().putString("language","es").commit();Messages.init(this);}}
