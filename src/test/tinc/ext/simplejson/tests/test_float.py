def test_floats():
    import simplejson
    for num in [1617161771.7650001]:
        assert simplejson.dumps(num) == repr(num)
