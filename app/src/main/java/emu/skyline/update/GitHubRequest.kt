/*
 * ====================================================================
 * Copyright (c) 2012-2023 AbandonedCart.  All rights reserved.
 *
 * See https://github.com/SamSprung/.github/blob/main/LICENSE#L5
 * ====================================================================
 *
 * The license and distribution terms for any publicly available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution license
 * [including the GNU Public License.] Content not subject to these terms is
 * subject to to the terms and conditions of the Apache License, Version 2.0.
 */

package emu.skyline.update

import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.*
import java.net.URL
import java.nio.charset.StandardCharsets
import javax.net.ssl.HttpsURLConnection

class GitHubRequest @JvmOverloads constructor(val url: String, val type: Content, val output: File? = null) {

    private lateinit var listener: ResultListener

    enum class Content {
        JSON,
        ASSET
    }

    @get:Throws(IOException::class)
    private val URL.asConnection get() : HttpsURLConnection {
        return (openConnection() as HttpsURLConnection).apply {
            requestMethod = "GET"
            useCaches = false
            defaultUseCaches = false
        }
    }

    private val HttpsURLConnection.withToken get() : HttpsURLConnection {
        setRequestProperty("Authorization", "Bearer $token")
        return this
    }

    init {
        CoroutineScope(Dispatchers.IO).launch(Dispatchers.IO) {
            if (type == Content.JSON) {
                try {
                    var conn = URL(url).asConnection.withToken
                    var responseCode = conn.responseCode
                    if (responseCode == HttpsURLConnection.HTTP_MOVED_PERM) {
                        val address = conn.getHeaderField("Location")
                        conn.disconnect()
                        conn = URL(address).asConnection.withToken
                        responseCode = conn.responseCode
                    }
                    if (responseCode != HttpsURLConnection.HTTP_OK) {
                        conn.disconnect()
                        return@launch
                    }
                    conn.inputStream.use { inStream ->
                        BufferedReader(
                            InputStreamReader(inStream, StandardCharsets.UTF_8)
                        ).use { streamReader ->
                            val responseStrBuilder = StringBuilder()
                            var inputStr : String?
                            while (null != streamReader.readLine()
                                    .also { inputStr = it }
                            ) responseStrBuilder.append(inputStr)
                            listener.onResults(responseStrBuilder.toString())
                            conn.disconnect()
                        }
                    }
                } catch (e : IOException) {
                    e.printStackTrace()
                }
            } else if (type == Content.ASSET) {
                URL(url).asConnection.withToken.run {
                    setRequestProperty("Accept", "application/octet-stream")
                    inputStream.use { stream ->
                        FileOutputStream(output).use { stream.copyTo(it) }
                        disconnect()
                    }
                }
                output?.let { listener.onResults(it.toUri()) }
            }
        }
    }

    interface ResultListener {
        fun onResults(result: Any)
    }

    fun setResultListener(listener: ResultListener) {
        this.listener = listener
    }

    companion object {
        private const val hex = "6769746875625f7061745f31314141493654474930474251456774343350684b395f46465263516274703561336c473773724451514567494261744e436f7a7a646c4739487a30355174423674573537504645344c50766a476a617158"
        private val token: String get() {
            val output = StringBuilder()
            var i = 0
            while (i < hex.length) {
                val str = hex.substring(i, i + 2)
                output.append(str.toInt(16).toChar())
                i += 2
            }
            return output.toString()
        }
    }
}
