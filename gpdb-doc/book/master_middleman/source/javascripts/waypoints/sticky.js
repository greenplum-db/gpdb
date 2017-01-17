(function() {
  'use strict'

  var Waypoint = window.Waypoint;
  var adapter = Waypoint.Adapter;

  /* http://imakewebthings.com/waypoints/shortcuts/sticky-elements */
  function Sticky(options) {
    this.options = adapter.extend({}, Waypoint.defaults, Sticky.defaults, options)
    this.element = this.options.element
    this.$element = new adapter(this.element)
    this.createWrapper()
    this.createWaypoint()
  }

  /* Private */
  Sticky.prototype.createWaypoint = function() {
    var originalHandler = this.options.handler

    this.waypoint = new Waypoint(adapter.extend({}, this.options, {
      element: this.wrapper,
      handler: adapter.proxy(function(direction) {
        var shouldBeStuck = this.options.direction.indexOf(direction) > -1
        var wrapperHeight = shouldBeStuck ? this.$element.outerHeight(true) : ''

        this.$wrapper.height(wrapperHeight)
        this.$element.toggleClass(this.options.stuckClass, shouldBeStuck)

        if (originalHandler) {
          originalHandler.call(this, direction)
        }
      }, this)
    }))
  }

  /* Private */
  Sticky.prototype.createWrapper = function() {
    if (this.options.wrapper) {
      this.$element.wrap(this.options.wrapper)
    }
    this.$wrapper = this.$element.parent()
    this.wrapper = this.$wrapper.element
  }

  /* Public */
  Sticky.prototype.destroy = function() {
    if (this.$element.parent().element === this.wrapper) {
      this.waypoint.destroy()
      this.$element.removeClass(this.options.stuckClass)
      if (this.options.wrapper) {
        this.$element.unwrap()
      }
    }
  }

  Sticky.defaults = {
    wrapper: '<div class="sticky-wrapper" />',
    stuckClass: 'stuck',
    direction: 'down right'
  }

  Waypoint.Sticky = Sticky
}())
