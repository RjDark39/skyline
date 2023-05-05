/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import emu.skyline.SkylineApplication
import org.json.JSONException
import org.json.JSONObject


class HashMapPreference(private var prefName : String) {

    private val sharedPref : SharedPreferences = PreferenceManager.getDefaultSharedPreferences(SkylineApplication.instance)

    private val hashMap : HashMap<String, String> = hashMapOf()

    init {
        try {
            val jsonString = sharedPref.getString(prefName, JSONObject().toString())
            if (!jsonString.isNullOrEmpty()) {
                val jsonObject = JSONObject(jsonString)
                val keysItr = jsonObject.keys()
                while (keysItr.hasNext()) {
                    val key = keysItr.next()
                    val value = jsonObject.getString(key)
                    hashMap[key] = value
                }
            }
        } catch (e : JSONException) {
            e.printStackTrace()
        }
    }

    private fun savePreference(inputMap : Map<String?, String?>) {
        val jsonString = JSONObject(inputMap).toString()
        with(sharedPref.edit()) {
            remove(prefName)
            putString(prefName, jsonString)
            apply()
        }
    }

    fun removeValue(key : String) {
        hashMap.remove(key)
        savePreference(hashMap.toMap())
    }

    fun putValue(key : String, directory : String) {
        hashMap[key] = directory
        savePreference(hashMap.toMap())
    }

    private fun getValue(key : String) : String? {
        return hashMap[key]
    }
}
