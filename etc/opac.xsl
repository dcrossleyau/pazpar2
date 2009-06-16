<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:pz="http://www.indexdata.com/pazpar2/1.0"
    xmlns:marc="http://www.loc.gov/MARC21/slim">
  
  <xsl:import href="marc21.xsl"/>

  <xsl:output indent="yes" method="xml" version="1.0" encoding="UTF-8"/>

<!-- Extract metadata from OPAC records with embedded MAR records
      http://www.loc.gov/marc/bibliographic/ecbdhome.html
-->  

  <xsl:template name="record-hook">
    <xsl:for-each select="/opacRecord/holdings/holding">
      <pz:metadata type="locallocation">
        <xsl:value-of select="localLocation"/>
      </pz:metadata>
      <pz:metadata type="callnumber">
        <xsl:value-of select="callNumber"/>
      </pz:metadata>
      <pz:metadata type="publicnote">
        <xsl:value-of select="publicNote"/>
      </pz:metadata>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="/">
    <xsl:apply-templates select="opacRecord/bibliographicRecord"/>
  </xsl:template>

</xsl:stylesheet>
