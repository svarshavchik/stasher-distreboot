<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <xsl:param name="chunk.fast">1</xsl:param>
  <xsl:param name="use.id.as.filename">1</xsl:param>
  <xsl:param name="html.stylesheet">book.css</xsl:param>
  <xsl:param name="root.filename">toc</xsl:param>
  <xsl:param name="generate.id.attributes">1</xsl:param>
  <xsl:include href="http://docbook.sourceforge.net/release/xsl/current/xhtml-1_1/chunk.xsl" />
</xsl:stylesheet>
