<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:output omit-xml-declaration="yes" indent="no"/>
    <xsl:template match="/">
        <xsl:for-each select="/prices/pricerecord">
            <xsl:value-of select="itemnumber"/>|<xsl:value-of select="price"/><xsl:text>&#10;</xsl:text> <!-- newline character -->
        </xsl:for-each>
    </xsl:template>
    <xsl:strip-space elements="*"/>
</xsl:stylesheet>
