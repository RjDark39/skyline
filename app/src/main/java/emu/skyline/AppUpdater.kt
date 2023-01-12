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
import android.os.Handler
import android.os.Looper
import android.text.Html
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat.startActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.json.JSONArray
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

    init {
        if (File(SkylineApplication.instance.getPublicFilesDir().canonicalPath + "/skyline.apk").exists()) {
            File(SkylineApplication.instance.getPublicFilesDir().canonicalPath + "/skyline.apk").delete()
        }
    }

    companion object {
        private const val baseUrl = "https://skyline-builds.alula.gay"
        private const val branch = "ftx1"

        @JvmStatic
        fun checkForUpdates(applicationContext : Context) {
            val myHandler = Handler(Looper.getMainLooper())
            val builder = AlertDialog.Builder(applicationContext)

            val url = URL("$baseUrl/builds")
            CoroutineScope(Dispatchers.IO).launch {
                try {
                    val response = url.readText()
                    val jsonBuilds = JSONTokener(response).nextValue() as JSONArray

                    var ftx1Index = 0
                    while (ftx1Index < jsonBuilds.length() && !jsonBuilds.getJSONObject(ftx1Index).get("branch").equals(branch)) {
                        ftx1Index++
                    }
                    if (ftx1Index >= jsonBuilds.length())
                        ftx1Index = 0

                    val remoteBuildGitHash = jsonBuilds.getJSONObject(ftx1Index).getJSONObject("commit").getString("id")
                    if (!BuildConfig.GIT_HASH.equals(remoteBuildGitHash)) {
                        val id = jsonBuilds.getJSONObject(ftx1Index).get("id")
                        val apkName = jsonBuilds.getJSONObject(ftx1Index).get("apkName")
                        val uri = Uri.parse("$baseUrl/cache/${id}/${apkName}")

                        myHandler.post {
                            builder.setTitle("New version ${jsonBuilds.getJSONObject(ftx1Index).get("runNumber")}")
                                .setMessage(Html.fromHtml("<b>Changelog</b><p>${jsonBuilds.getJSONObject(ftx1Index).getJSONObject("commit").getString("message")}</p>", 0))
                                .setCancelable(true)
                                .setPositiveButton("Update") { dialogInterface, it ->
                                    val receiver = AppUpdater()
                                    applicationContext.registerReceiver(receiver, IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE))
                                    receiver.downloadApk(applicationContext, uri)
                                    dialogInterface.dismiss()
                                }.show()
                        }
                    } else {
                        myHandler.post {
                            builder.setTitle("No updates available")
                                .setMessage(Html.fromHtml("<b>Changelog</b><p>${jsonBuilds.getJSONObject(ftx1Index).getJSONObject("commit").getString("message")}</p>", 0))
                                .setCancelable(true)
                                .setPositiveButton("Close") { dialogInterface, it ->
                                    dialogInterface.dismiss()
                                }.show()
                        }
                    }
                } catch (e : IOException) {
                    e.printStackTrace()
                }
            }
        }
    }
}
