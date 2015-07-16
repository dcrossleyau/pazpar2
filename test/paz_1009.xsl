<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!-- expects AGService XML records -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:pz="http://www.indexdata.com/pazpar2/1.0" version="1.0">
  
  <xsl:template name="first-6-chars">
    <xsl:param name="subfield-value"/>
    <xsl:value-of select="substring($subfield-value,1,6)"/>
  </xsl:template>
  <xsl:template name="first-4-digits">
    <xsl:param name="subfield-value"/>
    <xsl:value-of select="substring(translate($subfield-value, translate($subfield-value,'0123456789', ''), ''),1,4)"/>
  </xsl:template>

  <xsl:output encoding="UTF-8" indent="yes" method="xml" version="1.0"/>
  
   <xsl:template name="medium">
    <!-- Default medium template that may be overridden from the init doc. -->
    <!-- display-medium from Illuminar -->
    <xsl:choose>
      <xsl:when test="MarcLeader">
        <!-- strings in xslt are 1-indexed -->
        <xsl:variable name="ldr06" select="substring(MarcLeader, 7, 1)" />
        <xsl:variable name="ldr07" select="substring(MarcLeader, 8, 1)" />
        <xsl:variable name="ldr08" select="substring(MarcLeader, 9, 1)" />
        <xsl:choose>
          <xsl:when test="$ldr06 = 'c'">
            <xsl:text>Notated music</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'd'">
            <xsl:text>Manuscript notated music</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'e'">
            <xsl:text>Cartographic material</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'f'">
            <xsl:text>Manuscript cartographic material</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'g'">
            <xsl:text>Projected medium</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'i'">
            <xsl:text>Nonmusical sound recording</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'j'">
            <xsl:text>Musical sound recording</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'k'">
            <xsl:text>Two-dimensional nonprojectable graphic</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'm'">
            <xsl:text>Computer file</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'o'">
            <xsl:text>Kit</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'p'">
            <xsl:text>Mixed materials</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr06 = 'r'">
            <xsl:text>Visual materials</xsl:text>
          </xsl:when>
          <xsl:when test="$ldr07 = 'b' or $ldr07 = 's'">
            <xsl:text>Serial</xsl:text>
          </xsl:when>
          <!-- by default a book -->
          <xsl:otherwise>
            <xsl:text>Book</xsl:text>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>xml records</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="Results">
    <collection>
      <xsl:apply-templates/>
    </collection>
  </xsl:template>
  
  <xsl:template match="Result">    
    <xsl:variable name="medium">
      <xsl:call-template name="medium"/>
    </xsl:variable>

    <pz:record>
      <pz:metadata type="medium">
        <xsl:call-template name="medium"/>
      </pz:metadata>
      <xsl:for-each select="Title">
        <pz:metadata type="title">
          <xsl:choose>
            <xsl:when test="position()=1">
              <xsl:value-of select="."/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="concat(' ',.)"/>
            </xsl:otherwise>
          </xsl:choose>
        </pz:metadata>
      </xsl:for-each>
      <xsl:for-each select="Author">
        <pz:metadata type="author">
          <xsl:choose>
            <xsl:when test="position()=1">
              <xsl:value-of select="."/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="concat(' ',.)"/>
            </xsl:otherwise>
          </xsl:choose>
        </pz:metadata>
      </xsl:for-each>
      <xsl:for-each select="Subject">
        <pz:metadata type="subject">
          <xsl:choose>
            <xsl:when test="position()=1">
              <xsl:value-of select="."/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="concat(' ',.)"/>
            </xsl:otherwise>
          </xsl:choose>
        </pz:metadata>
      </xsl:for-each>
      <xsl:for-each select="PubYear">
        <pz:metadata type="date">
          <xsl:choose>
            <xsl:when test="position()=1">
              <xsl:value-of select="."/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="concat(' ',.)"/>
            </xsl:otherwise>
          </xsl:choose>
        </pz:metadata>
      </xsl:for-each>
      <xsl:choose>
        <xsl:when test="$medium = 'books'">
          <xsl:for-each select="Title">
            <pz:metadata type="brief-2">
              <xsl:choose>
                <xsl:when test="position()=1">
                  <xsl:value-of select="."/>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="concat(' ',.)"/>
                </xsl:otherwise>
              </xsl:choose>
            </pz:metadata>
          </xsl:for-each>
          <xsl:for-each select="Author">
            <pz:metadata type="brief-3">
              <xsl:choose>
                <xsl:when test="position()=1">
                  <xsl:value-of select="."/>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="concat(' ',.)"/>
                </xsl:otherwise>
              </xsl:choose>
            </pz:metadata>
          </xsl:for-each>
        </xsl:when>
      </xsl:choose>
    </pz:record>

  </xsl:template>
  
  <xsl:template match="text()"/>

</xsl:stylesheet>