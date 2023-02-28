/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 * Copyright © 2023 AbandonedCart
 */

package emu.skyline.update

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.content.res.Resources
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.TypedValue
import android.widget.ImageView
import android.widget.Toast
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.ActivityResultRegistry
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.res.use
import androidx.documentfile.provider.DocumentFile
import com.google.android.material.badge.BadgeDrawable
import com.google.android.material.badge.BadgeUtils
import emu.skyline.BuildConfig
import emu.skyline.R
import emu.skyline.SkylineApplication
import emu.skyline.getPublicFilesDir
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import org.json.JSONTokener
import java.io.File
import java.io.IOException
import kotlin.random.Random

object UpdateManager {

    private val Number.toPx get() = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, this.toFloat(), Resources.getSystem().displayMetrics).toInt()

    private const val appName = "skyline"
    private var onRequestInstall: ActivityResultLauncher<Intent>? = null
    private var listener : UpdateListener? = null

    init {
        CoroutineScope(Dispatchers.IO).launch(Dispatchers.IO) {
            SkylineApplication.instance.run {
                with (packageManager.packageInstaller) {
                    mySessions.forEach {
                        try {
                            abandonSession(it.sessionId)
                        } catch (ignored : Exception) { }
                    }
                }
                val apkFile = File(getPublicFilesDir().canonicalPath, "$appName.apk")
                if (apkFile.exists()) apkFile.delete()
            }
        }
    }

    private fun installDownload(context: Context, apkUri: Uri) {
        CoroutineScope(Dispatchers.IO).launch(Dispatchers.IO) {
            try {
                context.applicationContext.run {
                    contentResolver.openInputStream(apkUri).use { apkStream ->
                        val session = with (packageManager.packageInstaller) {
                            val params = PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL)
                            openSession(createSession(params))
                        }
                        val document = DocumentFile.fromSingleUri(context, apkUri) ?: throw IOException()
                        session.openWrite("NAME", 0, document.length()).use { sessionStream ->
                            apkStream?.copyTo(sessionStream)
                            session.fsync(sessionStream)
                        }
                        val pi = PendingIntent.getBroadcast(this, Random.nextInt(),
                            Intent(this, UpdateReceiver::class.java),
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE else PendingIntent.FLAG_UPDATE_CURRENT
                        )
                        session.commit(pi.intentSender)
                    }
                }
            } catch (ex : SecurityException) {
                ex.printStackTrace()
            } catch (ex : IOException) {
                ex.printStackTrace()
            }
        }
    }

    private var downloadUrl: String? = null

    private fun requestDownload(context: Context) {
        CoroutineScope(Dispatchers.IO).launch(Dispatchers.IO) {
            val apkFile = File(context.getPublicFilesDir().canonicalPath, "$appName.apk")
            if (apkFile.exists()) apkFile.delete()
            GitHubRequest(downloadUrl!!, GitHubRequest.Content.ASSET, apkFile).setResultListener(object : GitHubRequest.ResultListener {
                override fun onResults(result : Any) {
                    installDownload(context, result as Uri)
                }
            })
        }
    }

    private fun getDownloadOrNull(context: Context, manual: Boolean) {
        GitHubRequest("https://api.github.com/repos/8bitDream/$appName/releases/tags/8bit/ftx1", GitHubRequest.Content.JSON).setResultListener(object : GitHubRequest.ResultListener {
            override fun onResults(result : Any) {
                CoroutineScope(Dispatchers.IO).launch(Dispatchers.IO) {
                    try {
                        val jsonObject = JSONTokener(result as String).nextValue() as JSONObject
                        val lastCommit = (jsonObject["name"] as String).substring(appName.length + 1)
                        if (!BuildConfig.GIT_HASH.startsWith(lastCommit)) {
                            val assets = jsonObject["assets"] as JSONArray
                            val asset = assets[0] as JSONObject
                            downloadUrl = asset["url"] as String
                        }
                        if (downloadUrl.isNullOrEmpty()) {
                            if (manual)
                                CoroutineScope(Dispatchers.Main).launch {
                                    Toast.makeText(context, R.string.no_updates_available, Toast.LENGTH_SHORT).show()
                                }
                            else
                                listener?.onNoUpdateAvailable()
                        } else {
                            if (manual)
                                requestDownload(context)
                            else
                                listener?.onUpdateFound(downloadUrl!!)
                        }
                    } catch (e : JSONException) {
                        e.printStackTrace()
                        if (!manual) listener?.onNoUpdateAvailable()
                    }
                }
            }
        })
    }

    private fun checkForUpdate(context: Context) {
        if (downloadUrl.isNullOrEmpty()) {
            getDownloadOrNull(context, true)
        } else {
            requestDownload(context)
        }
    }

    fun requestUpdateManager(context: Context, registry: ActivityResultRegistry) {
        if (context.packageManager.canRequestPackageInstalls()) {
            checkForUpdate(context)
        } else {
            onRequestInstall = registry.register("UpdateManager", ActivityResultContracts.StartActivityForResult()) {
                if (context.packageManager.canRequestPackageInstalls()) checkForUpdate(context)
                onRequestInstall?.unregister()
                onRequestInstall = null
            }
            val intent = Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES)
            intent.data = Uri.parse(String.format("package:%s", context.packageName))
            onRequestInstall?.launch(intent)
        }
    }

    @com.google.android.material.badge.ExperimentalBadgeUtils
    fun applyUpdateBadge(icon : ImageView) {
        setUpdateListener(object : UpdateListener {
            override fun onUpdateFound(downloadUrl : String) {
                UpdateManager.downloadUrl = downloadUrl
                CoroutineScope(Dispatchers.IO).launch(Dispatchers.IO) {
                    val context = icon.context
                    BadgeDrawable.create(context).apply {
                        badgeGravity = BadgeDrawable.TOP_END
                        verticalOffset = 8.toPx
                        horizontalOffset = 6.toPx
                        backgroundColor = context.obtainStyledAttributes(intArrayOf(R.attr.colorPrimary)).use { it.getColor(0, Color.GREEN) }
                    }.also {
                        BadgeUtils.attachBadgeDrawable(it, icon)
                    }
                }
            }

            override fun onNoUpdateAvailable() { }
        })
        getDownloadOrNull(icon.context, false)
    }

    private fun setUpdateListener(listener : UpdateListener?) {
        this.listener = listener
    }

    interface UpdateListener {
        fun onUpdateFound(downloadUrl: String)
        fun onNoUpdateAvailable()
    }
}
