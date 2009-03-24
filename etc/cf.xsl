<?xml version="1.0" encoding="UTF-8"?>
<!--

    This stylesheet expects Connector Frameworks records
    $Id: usco.xsl,v 1.3 2009-01-13 15:18:42 wosch Exp $

-->
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:siebel="http://loc.gov/siebel/elements/1.0/" >

 <xsl:output indent="yes"
        method="xml"
        version="1.0"
        encoding="UTF-8"/>

  <xsl:template match="/record">
    <pz:record>

      <xsl:attribute name="mergekey">
	<xsl:text>title </xsl:text>
        <xsl:value-of select="title" />
	<xsl:text> author </xsl:text>
        <xsl:value-of select="author"/>
      </xsl:attribute>

      <pz:metadata type="id">
        <xsl:value-of select="url"/>
      </pz:metadata>

      <pz:metadata type="author">
        <xsl:value-of select="author"/>
      </pz:metadata>

        <pz:metadata type="title">
          <xsl:value-of select="title" />
        </pz:metadata>

        <pz:metadata type="date">
          <xsl:value-of select="date" />
        </pz:metadata>

        <pz:metadata type="electronic-url">
          <xsl:value-of select="url" />
        </pz:metadata>

    </pz:record>
  </xsl:template>

  <xsl:template match="text()"/>

</xsl:stylesheet>
