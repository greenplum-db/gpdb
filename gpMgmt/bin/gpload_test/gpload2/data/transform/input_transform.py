#!/usr/bin/env python3
import sys
import lxml.etree as ET

data = ET.parse(sys.argv[1])
xslt = ET.parse("data/transform/input_transform.xslt")
transform = ET.XSLT(xslt)
result = transform(data)
content = [line for line in str(result).split('\n') if line.strip() != '']
print("\n".join(content))
