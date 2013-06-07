/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import QtTest 1.0
import ".."
import "../../../Panel"
import Ubuntu.Components 0.1
import Unity.Test 0.1 as UT

Rectangle {
    width: units.gu(10)
    height: units.gu(5)
    color: "black"

    IndicatorItem {
        id: indicatorItem
        anchors.fill: parent
    }

    UT.UnityTestCase {
        name: "IndicatorItem"

        function init_test() {
        }

        function test_dimmed() {
            init_test()

            indicatorItem.dimmed = true
            compare(indicatorItem.opacity > 0, true, "IndicatorItem opacity should not be 0")
            compare(indicatorItem.opacity < 1, true, "IndicatorItem opacity should not be 1")
        }

        function test_highlight() {
            var itemHighlight = findChild(indicatorItem, "highlight")
            verify(itemHighlight != undefined)

            indicatorItem.highlighted = true;
            compare(itemHighlight.visible, true, "Indicator should be highlighted")

            indicatorItem.highlighted = false;
            compare(itemHighlight.visible, false, "Indicator should not be highlighted")
        }

        function test_empty() {
            init_test()

            compare(indicatorItem.visible, false, "IndicatorItem should not be visible.")
            indicatorItem.iconQml = "qrc:/tests/indciatorsclient/qml/fake_menu_icon1.qml";
            tryCompare(indicatorItem, "visible", true)
        }
    }
}
