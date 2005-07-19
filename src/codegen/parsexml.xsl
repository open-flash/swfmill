<?xml version="1.0"?>
<xsl:stylesheet	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>
	<xsl:template name="parsexml">
<xsl:document href="g{/format/@format}ParseXML.cpp" method="text">

#include "<xsl:value-of select="/format/@format"/>.h"
#include &lt;string.h&gt;
#include &lt;ctype.h&gt;
#include "base64.h"

namespace <xsl:value-of select="/format/@format"/> {

<xsl:for-each select="type|tag|action|style|stackitem">
void <xsl:value-of select="@name"/>::parseXML( xmlNodePtr node, Context *ctx ) {
	xmlNodePtr node2;
	xmlChar *tmp;
	
//	printf("<xsl:value-of select="@name"/>::parseXML\n");

	<xsl:for-each select="*[@context]">
		ctx-&gt;<xsl:value-of select="@name"/> = <xsl:apply-templates select="." mode="default"/>;
	</xsl:for-each>

	<xsl:apply-templates select="*[@prop]|flagged|if|context" mode="parsexml"/>
	
	<xsl:if test="@name='UnknownTag' or @name='UnknownAction'">
		tmp = xmlGetProp( node, (const xmlChar *)"id" );
		if( tmp ) { 
			sscanf( (char *)tmp, "%X", &amp;type ); 
			xmlFree( (xmlChar *)tmp ); 
		}
	</xsl:if>

	<xsl:for-each select="*[@context]">
		<xsl:choose>
			<xsl:when test="@context='inverse'">
				ctx-><xsl:value-of select="@name"/> = <xsl:value-of select="@name"/>;
			</xsl:when>
			<xsl:when test="@set-from-bits-needed">
				<xsl:value-of select="@name"/> = SWFBitsNeeded( <xsl:value-of select="@set-from-bits-needed"/> );
				if( <xsl:value-of select="@name"/> > ctx-><xsl:value-of select="@name"/> ) ctx-><xsl:value-of select="@name"/> = <xsl:value-of select="@name"/>;
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="@name"/> = ctx-><xsl:value-of select="@name"/>;
			</xsl:otherwise>
		</xsl:choose>
	</xsl:for-each>
}
</xsl:for-each>

}

</xsl:document>
	</xsl:template>


<xsl:template match="byte|word|fixedpoint|bit|integer|string|uint32|float|double" mode="has">
	if( <xsl:if test="../@negative">!</xsl:if>xmlHasProp( node, (const xmlChar *)"<xsl:value-of select="@name"/>" ) ) has = true;
</xsl:template>

<xsl:template match="object|list|data" mode="has">
	{
		xmlNodePtr child = node->children;
		while( child &amp;&amp; !has ) {
			if( !strcmp( (const char *)child->name, "<xsl:value-of select="@name"/>" ) ) has = true;
			child = child->next;
		}
	}
</xsl:template>


<xsl:template match="flagged" mode="parsexml">
	{
		bool has = false;
		<xsl:for-each select="*[@prop]|flagged|if">
			<xsl:apply-templates select="." mode="has"/>
			<xsl:choose>
				<xsl:when test="../@signifier">
					if( has ) <xsl:value-of select="../@flag"/> |= <xsl:value-of select="$signifier"/>;
					else <xsl:value-of select="../@flag"/> ^= <xsl:value-of select="$signifier"/>;
				</xsl:when>
				<xsl:otherwise>
					if( has ) <xsl:value-of select="../@flag"/> = <xsl:if test="../@negative">!</xsl:if>true;
				</xsl:otherwise>
			</xsl:choose>
		</xsl:for-each>
		
		if( has ) {
			<xsl:apply-templates select="*[@prop]|flagged|if" mode="parsexml"/>
		}
	
		<xsl:for-each select="*[@context]">
			<xsl:value-of select="@name"/> = ctx-><xsl:value-of select="@name"/>;
		</xsl:for-each>
	}
</xsl:template>

<xsl:template match="if" mode="parsexml">
	if( <xsl:value-of select="@expression"/> ) {
		<xsl:apply-templates select="*[@prop]|flagged|if" mode="parsexml"/>
	}
</xsl:template>


<xsl:template match="byte|word|bit|uint32" mode="parsexml">
	tmp = xmlGetProp( node, (const xmlChar *)"<xsl:value-of select="@name"/>" );
	if( tmp ) {
		int tmp_int;
		sscanf( (char *)tmp, "<xsl:apply-templates select="." mode="printf"/>", &amp;tmp_int );
		<xsl:value-of select="@name"/> = tmp_int;
		xmlFree( tmp );
	}
</xsl:template>

<xsl:template match="float|double" mode="parsexml">
	tmp = xmlGetProp( node, (const xmlChar *)"<xsl:value-of select="@name"/>" );
	if( tmp ) {
		float tmp_float;
		sscanf( (char *)tmp, "%f", &amp;tmp_float );
		<xsl:value-of select="@name"/> = tmp_float;
		xmlFree( tmp );
	}
</xsl:template>

<xsl:template match="fixedpoint" mode="parsexml">
	tmp = xmlGetProp( node, (const xmlChar *)"<xsl:value-of select="@name"/>" );
	if( tmp ) {
		float t;
		sscanf( (char *)tmp, "<xsl:apply-templates select="." mode="printf"/>", &amp;t);
		<xsl:value-of select="@name"/> = t;
		xmlFree( tmp );
		<xsl:choose>
		<!-- should this be done in writer.xsl? -->
			<xsl:when test="@constant-size"/>
			<xsl:otherwise>
				int b = SWFBitsNeeded( (long)(<xsl:value-of select="@name"/>*(1&lt;&lt; <xsl:value-of select="@exp"/>))<xsl:if test="@signed">, true</xsl:if> );
				<xsl:if test="@size-add">b -= <xsl:value-of select="@size-add"/>;</xsl:if>
				if( b > <xsl:value-of select="@size"/> ) <xsl:value-of select="@size"/> = b;
			</xsl:otherwise>
		</xsl:choose>
	} else {
		fprintf(stderr,"WARNING: no <xsl:value-of select="@name"/> property in %s element\n", (const char *)node->name );
	}
</xsl:template>

<xsl:template match="integer" mode="parsexml">
	tmp = xmlGetProp( node, (const xmlChar *)"<xsl:value-of select="@name"/>" );
	if( tmp ) {
		sscanf( (char *)tmp, "<xsl:apply-templates select="." mode="printf"/>", &amp;<xsl:value-of select="@name"/>);
		xmlFree( tmp );
		<xsl:choose>
		<!-- should this be done in writer.xsl? -->
			<xsl:when test="@constant-size"/>
			<xsl:otherwise>
				int b = SWFBitsNeeded( <xsl:value-of select="@name"/><xsl:if test="@signed">, true</xsl:if> );
				<xsl:if test="@size-add">b -= <xsl:value-of select="@size-add"/>;</xsl:if>
				if( b > <xsl:value-of select="@size"/> ) <xsl:value-of select="@size"/> = b;
			</xsl:otherwise>
		</xsl:choose>
	} else {
		fprintf(stderr,"WARNING: no <xsl:value-of select="@name"/> property in %s element\n", (const char *)node->name );
	}
</xsl:template>

<xsl:template match="string" mode="parsexml">
	<!-- FIXME: standardize string handling on xmlString. this should be deleted somewhere, and checked... -->
	tmp = xmlGetProp( node, (const xmlChar *)"<xsl:value-of select="@name"/>" );
	if( tmp ) {
		<xsl:value-of select="@name"/> = strdup((const char *)tmp);
	} else {
		fprintf(stderr,"WARNING: no <xsl:value-of select="@name"/> property in %s element\n", (const char *)node->name );
		<xsl:value-of select="@name"/> = strdup("[undefined]");
	}
</xsl:template>

<xsl:template match="object" mode="parsexml">
	node2 = node->children;
	while( node2 ) {
		if( !strcmp( (const char *)node2->name, "<xsl:value-of select="@name"/>" ) ) {
<!--
			<xsl:value-of select="@name"/>.parseXML( node2, ctx );
			node=NULL;
-->
		xmlNodePtr child = node2->children;
			while( child ) {
				if( !xmlNodeIsText( child ) ) {
					<xsl:value-of select="@name"/>.parseXML( child, ctx );
					node2 = child = NULL;
					node2 = NULL;
				} else {
					child = child->next;
				}
			}
		}
		if( node2 ) node2 = node2->next;
	}
</xsl:template>

<xsl:template match="list" mode="parsexml">
	node2 = node->children;
	while( node2 ) {
		if( !strcmp( (const char *)node2->name, "<xsl:value-of select="@name"/>" ) ) {
			<xsl:if test="@length">
				<xsl:value-of select="@length"/>=0;
			</xsl:if>
			
			xmlNodePtr child = node2->children;
			while( child ) {
				if( !xmlNodeIsText( child ) ) {
					<xsl:value-of select="@type"/>* item = <xsl:value-of select="@type"/>::getByName( (const char *)child->name );
					if( item ) {
						item->parseXML( child, ctx );
						<xsl:value-of select="@name"/>.append( item );
						<xsl:if test="@length">
							<xsl:value-of select="@length"/>++;
						</xsl:if>
					}
				}
				child = child->next;
			}
			
			node2=NULL;
		} else {
			node2 = node2->next;
		}
	}
</xsl:template>

<xsl:template match="data" mode="parsexml">
	{
		<xsl:value-of select="@name"/> = NULL;
		<xsl:value-of select="@size"/> = 0;

		xmlChar *xmld = xmlNodeGetContent( node );
		char *d = (char *)xmld;
		if( d ) {
			// unsure if this is neccessary
			for( int i=strlen(d)-1; i>0 &amp;&amp; isspace(d[i]); i-- ) d[i]=0;
			while( isspace(d[0]) ) d++;
			int l = strlen(d); //BASE64_GET_MAX_DECODED_LEN(strlen( d ));
			char *dst = new char[ l ];
			int lout = base64_decode( dst, (char*)d, l );
			if( lout > 0 ) {
				<xsl:value-of select="@size"/> = lout;
				<xsl:value-of select="@name"/> = new unsigned char[ lout ];
				memcpy( <xsl:value-of select="@name"/>, dst, lout );
			}
			delete dst;
			xmlFree( xmld );
		} 
	}
</xsl:template>

<xsl:template match="context" mode="parsexml">
	ctx-><xsl:value-of select="@param"/> = <xsl:value-of select="@value"/>;
</xsl:template>

</xsl:stylesheet>