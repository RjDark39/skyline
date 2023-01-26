/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.app.DownloadManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.text.Html
import android.widget.ImageView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.content.ContextCompat.startActivity
import com.google.android.material.badge.BadgeDrawable
import com.google.android.material.badge.BadgeDrawable.BOTTOM_END
import com.google.android.material.badge.BadgeUtils
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import org.json.JSONTokener
import java.io.File
import java.io.IOException
import java.net.URL

class AppUpdater : BroadcastReceiver() {
    private var downloadID = 0L

    override fun onReceive(context : Context, intent : Intent) {
        //Fetching the download id received with the broadcast
        val id = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1)
        //Checking if the received broadcast is for our enqueued download by matching download id
        if (downloadID == id) {
            Toast.makeText(context, "Download Completed", Toast.LENGTH_SHORT).show()

            val intentInstall = Intent(Intent.ACTION_INSTALL_PACKAGE)
            intentInstall.data = (context.getSystemService(AppCompatActivity.DOWNLOAD_SERVICE) as DownloadManager).getUriForDownloadedFile(downloadID)
            intentInstall.flags = Intent.FLAG_GRANT_READ_URI_PERMISSION
            startActivity(context, intentInstall, null)
            context.unregisterReceiver(this)
        }
    }

    fun downloadApk(applicationContext : Context, uri : Uri) {
        val downloadPath = applicationContext.getPublicFilesDir().canonicalPath
        val file = File(downloadPath, "skyline.apk")

        if (File(file.path).exists()) {
            File(file.path).delete()
        }

        val request = DownloadManager.Request(uri).setTitle("skyline")
            .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE)
            .setDestinationUri(Uri.fromFile(file))

        val downloadManager = applicationContext.getSystemService(AppCompatActivity.DOWNLOAD_SERVICE) as DownloadManager
        downloadID = downloadManager.enqueue(request)
    }

    companion object {
        private const val baseUrl = "https://skyline-builds.alula.gay"
        private const val branch = "ftx1"

        fun checkForUpdates(applicationContext : Context) {
            val builder = AlertDialog.Builder(applicationContext)

            CoroutineScope(Dispatchers.IO).launch {
                val newestBuild = checkRemoteForUpdates()
                if (newestBuild != null) {
                    val commit = newestBuild.getJSONObject("commit")
                    val remoteBuildGitHash = commit.getString("id")
                    if (BuildConfig.GIT_HASH != remoteBuildGitHash) {
                        val id = newestBuild.get("id")
                        val apkName = newestBuild.get("apkName")
                        val uri = Uri.parse("$baseUrl/cache/${id}/${apkName}")

                        val message = commit.getString("message")
                        var changelog = "<b>${applicationContext.getString(R.string.changelog)}</b><p>${message.substringBefore("\n")}</p>"
                        if (message.contains("\n"))
                            changelog += "<p>${message.substringAfter("\n")}</p>"

                        withContext(Dispatchers.Main) {
                            builder.setTitle("${applicationContext.getString(R.string.new_version)} ${newestBuild.get("runNumber")}")
                                .setMessage(Html.fromHtml(changelog, 0))
                                .setCancelable(true)
                                .setPositiveButton(applicationContext.getString(R.string.update)) { dialogInterface, _ ->
                                    val receiver = AppUpdater()
                                    applicationContext.registerReceiver(receiver, IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE))
                                    receiver.downloadApk(applicationContext, uri)
                                    dialogInterface.dismiss()
                                }.setNegativeButton(applicationContext.getString(R.string.cancel)){ dialogInterface, _ ->
                                    dialogInterface.cancel()
                                }.show()
                        }
                    } else {
                        withContext(Dispatchers.Main) {
                            builder.setTitle(applicationContext.getString(R.string.no_updates_available))
                                .setMessage(Html.fromHtml("<b>${applicationContext.getString(R.string.changelog)}</b><p>${commit.getString("message")}</p>", 0))
                                .setCancelable(true)
                                .setPositiveButton(applicationContext.getString(R.string.close)) { dialogInterface, _ ->
                                    dialogInterface.dismiss()
                                }.show()
                        }
                    }
                }
            }
        }

        @com.google.android.material.badge.ExperimentalBadgeUtils
        fun notifyUpdateBadge(context : Context, icon : ImageView) {
            CoroutineScope(Dispatchers.IO).launch {
                val newestBuild = checkRemoteForUpdates()
                if (newestBuild != null) {
                    val remoteBuildGitHash = newestBuild.getJSONObject("commit").getString("id")
                    if (BuildConfig.GIT_HASH != remoteBuildGitHash) {
                        val badge = BadgeDrawable.create(context)
                        badge.badgeGravity = BOTTOM_END
                        badge.verticalOffset = 25
                        badge.horizontalOffset = 25
                        badge.backgroundColor = ContextCompat.getColor(context, R.color.colorPrimary)
                        BadgeUtils.attachBadgeDrawable(badge, icon)
                    }
                }
            }
        }

        private fun checkRemoteForUpdates() : JSONObject? {
            val url = URL("$baseUrl/builds")
            try {
                val response = url.readText()
                val jsonBuilds = JSONTokener(response).nextValue() as JSONArray

                var ftx1Index = 0
                while (ftx1Index < jsonBuilds.length() && jsonBuilds.getJSONObject(ftx1Index).get("branch") != branch) {
                    ftx1Index++
                }
                if (ftx1Index >= jsonBuilds.length())
                    ftx1Index = 0

                return jsonBuilds.getJSONObject(ftx1Index)
            } catch (e : IOException) {
                e.printStackTrace()
                return null
            }
        }

        fun removeApk(){
            if (File(SkylineApplication.instance.getPublicFilesDir().canonicalPath + "/skyline.apk").exists()) {
                File(SkylineApplication.instance.getPublicFilesDir().canonicalPath + "/skyline.apk").delete()
            }
        }
    }
}
