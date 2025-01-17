// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-toggle', function() {
  var toggle;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-toggle id="toggle"></cr-toggle>
    `;

    toggle = document.getElementById('toggle');
    assertNotChecked();
  });

  function assertChecked() {
    assertTrue(toggle.checked);
    assertTrue(toggle.hasAttribute('checked'));
    assertEquals('true', toggle.getAttribute('aria-pressed'));
    // Asserting that the toggle button has actually moved.
    assertTrue(getComputedStyle(toggle.$.button).transform.includes('matrix'));
  }

  function assertNotChecked() {
    assertFalse(toggle.checked);
    assertEquals(null, toggle.getAttribute('checked'));
    assertEquals('false', toggle.getAttribute('aria-pressed'));
    // Asserting that the toggle button has not moved.
    assertEquals('none', getComputedStyle(toggle.$.button).transform);
  }

  function assertDisabled() {
    assertTrue(toggle.disabled);
    assertTrue(toggle.hasAttribute('disabled'));
    assertEquals('true', toggle.getAttribute('aria-disabled'));
  }

  function assertNotDisabled() {
    assertFalse(toggle.disabled);
    assertFalse(toggle.hasAttribute('disabled'));
    assertEquals('false', toggle.getAttribute('aria-disabled'));
  }

  /** @param {string} keyName The name of the key to trigger. */
  function triggerKeyPressEvent(keyName) {
    // Note: MockInteractions incorrectly populates |keyCode| and |code| with
    // the same value. Since the prod code only cares about |code| being 'Enter'
    // or 'Space', passing a string as a 2nd param, instead of a number.
    MockInteractions.keyEventOn(
        toggle, 'keypress', keyName, undefined, keyName);
  }

  /**
   * Simulates dragging the toggle button left/right.
   * @param {number} moveDirection -1 for left, 1 for right, 0 when no
   *     pointermove event should be simulated.
   */
  function triggerPointerDownMoveUpTapSequence(moveDirection) {
    if (window.getComputedStyle(toggle)['pointer-events'] === 'none')
      return;

    // Simulate events in the same order they are fired by the browser.
    // Need to provide a valid |pointerId| for setPointerCapture() to not throw
    // an error.
    toggle.dispatchEvent(new PointerEvent(
        'pointerdown', {pointerId: 1, clientX: 100}));
    if (moveDirection) {
      var clientX = moveDirection > 0 ? 150: 50;
      toggle.dispatchEvent(new PointerEvent(
          'pointermove', {pointerId: 1, clientX: clientX}));
    }
    toggle.dispatchEvent(new PointerEvent(
        'pointerup', {pointerId: 1, clientX: clientX}));
    MockInteractions.tap(toggle);
  }

  // Test that the control is toggled when the |checked| attribute is
  // programmatically changed.
  test('ToggleByAttribute', function() {
    test_util.eventToPromise('change', toggle).then(function() {
      // Should not fire 'change' event when state is changed programmatically.
      // Only user interaction should result in 'change' event.
      assertFalse(true);
    });

    toggle.checked = true;
    assertChecked();

    toggle.checked = false;
    assertNotChecked();
  });

  // Test that the control is toggled when the user taps on it.
  test('ToggleByPointerTap', function() {
    var whenChanged = test_util.eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    return whenChanged.then(function() {
      assertChecked();
      whenChanged = test_util.eventToPromise('change', toggle);
      triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
      return whenChanged;
    }).then(function() {
      assertNotChecked();
    });
  });

  // Test that the control is toggled when the user moves the pointer while
  // holding down.
  test('ToggleByPointerMove', function() {
    var whenChanged = test_util.eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(1 /* right */);
    return whenChanged.then(function() {
      assertChecked();
      whenChanged = test_util.eventToPromise('change', toggle);
      triggerPointerDownMoveUpTapSequence(-1 /* left */);
      return whenChanged;
    }).then(function() {
      assertNotChecked();
      whenChanged = test_util.eventToPromise('change', toggle);

      // Test simple tapping after having dragged.
      triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
      return whenChanged;
    }).then(function() {
      assertChecked();
    });
  });

  // Test that the control is toggled when the user presses the 'Enter' or
  // 'Space' key.
  test('ToggleByKey', function() {
    var whenChanged = test_util.eventToPromise('change', toggle);
    triggerKeyPressEvent('Enter');
    return whenChanged.then(function() {
      assertChecked();
      whenChanged = test_util.eventToPromise('change', toggle);
      triggerKeyPressEvent('Space');
    }).then(function() {
      assertNotChecked();
    });
  });

  // Test that the control is not affected by user interaction when disabled.
  test('ToggleWhenDisabled', function() {
    assertNotDisabled();
    toggle.disabled = true;
    assertDisabled();

    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    assertNotChecked();
    assertDisabled();

    toggle.disabled = false;
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    assertChecked();
  });
});
