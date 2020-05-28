/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.view.KeyEvent
import android.view.MotionEvent
import java.io.Serializable
import java.util.*

/**
 * This an abstract class for all host events that is inherited by all other event classes
 *
 * @param descriptor The device descriptor of the device this event originates from
 */
abstract class HostEvent(val descriptor : String = "") : Serializable {
    /**
     * The [toString] function is abstract so that the derived classes can return a proper string
     */
    abstract override fun toString() : String

    /**
     * The equality function is abstract so that equality checking will be for the derived classes rather than this abstract class
     */
    abstract override fun equals(other : Any?) : Boolean

    /**
     * The hash function is abstract so that hashes will be generated for the derived classes rather than this abstract class
     */
    abstract override fun hashCode() : Int
}

class KeyHostEvent(descriptor : String = "", val keyCode : Int) : HostEvent(descriptor) {
    /**
     * This returns the string representation of [keyCode]
     */
    override fun toString() : String = KeyEvent.keyCodeToString(keyCode)

    /**
     * This does some basic equality checking for the type of [other] and all members in the class
     */
    override fun equals(other : Any?) : Boolean = if (other is KeyHostEvent) this.descriptor == other.descriptor && this.keyCode == other.keyCode else false

    /**
     * This computes the hash for all members of the class
     */
    override fun hashCode() : Int = Objects.hash(descriptor, keyCode)
}

class MotionHostEvent(descriptor : String = "", val axis : Int, val polarity : Boolean) : HostEvent(descriptor) {
    /**
     * This returns the string representation of [axis] combined with [polarity]
     */
    override fun toString() : String = MotionEvent.axisToString(axis) + if (polarity) "+" else "-"

    /**
     * This does some basic equality checking for the type of [other] and all members in the class
     */
    override fun equals(other : Any?) : Boolean {
        return if (other is MotionHostEvent) this.descriptor == other.descriptor && this.axis == other.axis && this.polarity == other.polarity else false
    }

    /**
     * This computes the hash for all members of the class
     */
    override fun hashCode() : Int = Objects.hash(descriptor, axis, polarity)
}
