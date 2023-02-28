/*
 * ====================================================================
 * Copyright (c) 2021-2023 AbandonedCart.  All rights reserved.
 *
 * https://github.com/AbandonedCart/AbandonedCart/blob/main/LICENSE#L4
 * ====================================================================
 *
 * The license and distribution terms for any publicly available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution license
 * [including the GNU Public License.] Content not subject to these terms is
 * subject to to the terms and conditions of the Apache License, Version 2.0.
 */

package emu.skyline.update

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.os.Build
import android.os.Parcelable
import android.widget.Toast
import java.net.URISyntaxException

class UpdateReceiver : BroadcastReceiver() {

    private inline fun <reified T : Parcelable> Intent.parcelable(key: String): T? = when {
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU -> getParcelableExtra(key, T::class.java)
        else -> @Suppress("DEPRECATION") getParcelableExtra(key) as? T
    }
    
    override fun onReceive(context : Context, intent : Intent) {
        intent.setPackage(context.packageName)
        intent.flags = 0
        intent.data = null
        when (intent.getIntExtra(PackageInstaller.EXTRA_STATUS, -1)) {
            PackageInstaller.STATUS_PENDING_USER_ACTION -> {
                intent.parcelable<Intent>(Intent.EXTRA_INTENT)?.let {
                    try {
                        context.startActivity(Intent.parseUri(it.toUri(0), Intent.URI_ALLOW_UNSAFE).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                    } catch (ignored: URISyntaxException) { }
                }
            }
            PackageInstaller.STATUS_SUCCESS -> { }
            else -> {
                val error = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE)
                if (error?.contains("Session was abandoned") != true)
                    Toast.makeText(context, error, Toast.LENGTH_LONG).show()
            }
        }
    }
}
