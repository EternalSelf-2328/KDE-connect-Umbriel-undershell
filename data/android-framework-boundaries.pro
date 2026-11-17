# android-smsmms bundles old android.net classes. On a phone, Android loads its
# own versions. R8 must never inline their legacy field layouts into our code.
# In 0.4.1 LinkAddress.getAddress() became an access to a nonexistent `address`
# field on API 36, causing NoSuchFieldError while checking a LAN pairing request.
-keep,allowshrinking class android.net.** { *; }

# Preserve the app's authentication and route boundaries as auditable classes.
# The engine has an upstream keep rule; the companion needs the same boundary.
-keep class org.eternalself.umbrielremote.** { *; }
