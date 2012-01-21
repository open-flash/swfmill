#include "SWFFile.h"
#include "SWFReader.h"
#include "SWFWriter.h"
#include <cstring>
#include <cstdlib>
#include <zlib.h>
#include "XmlAutoPtr.h"
#include "XmlDocAutoPtr.h"

namespace SWF {

	File::File() {
		// "sane" defaults
		compressed = false;
		version = 7;
		length = 0;
	}

	void File::dump() {
		Context ctx;
		if (!header.get()) {
			fprintf(stderr,"no SWF loaded to save\n");
			return;
		}

		fprintf(stderr, "version %i, size %i, %scompressed\n", version, length, (compressed?"":"un"));
		ctx.swfVersion = version;
		header->dump(2, &ctx);
	}

	int File::load(FILE *fp, Context *ctx, unsigned int filesize) {
		if (!ctx) {
			auto_ptr<Context> autoContext(new Context);
			return load(fp, autoContext.get(), filesize);
		}

		char sig[3];

		// read sig, version
		if (fread(&sig, 3, 1, fp) != 1
				|| fread(&version, 1, 1, fp) != 1
				|| (strncmp(sig,"CWS",2) && strncmp(sig,"FWS",2))) {
			fprintf(stderr,"ERROR: input is no SWF\n");
			return 0;
		}

		// read length, should be endian-safe
		length = fgetc(fp);
		length += fgetc(fp)<<8;
		length += fgetc(fp)<<16;
		length += fgetc(fp)<<24;

		ctx->swfVersion = version;

		length -= 8;

		compressed = sig[0]=='C';

		if (length != filesize-8) {
			if (length > filesize-8 && !compressed) {
				/* this allows uncompressed SWFs with invalid filesize to be parsed. */
				fprintf(stderr,"WARNING: size specified in SWF (%i) != filesize (%i), using filesize-8.\n",
						length, filesize);
				length = filesize - 8;
			}
		}

		auto_ptr<unsigned char> data(new unsigned char[length]);
		if (!data.get()) {
			fprintf(stderr,"cannot load SWF to memory (size %i)\n",length);
			return 0;
		}

		if (compressed) {
			decompress(data.get(), length, fp);
		} else {
			if (fread(data.get(), length, 1, fp ) != 1) {
				fprintf(stderr,"could not load SWF to memory (%i, %c)\n",length,sig[0]);
				return 0;
			}
		}

		auto_ptr<Reader> reader(new Reader(data.get(), length));
		header = auto_ptr<Header>(new Header);

		header->parse(reader.get(), length, ctx);

		if (reader->getError() != SWFR_OK) {
			if (reader->getError() == SWFR_EOF) {
				fprintf(stderr,"WARNING: reached EOF while reading SWF\n");
			} else {
				fprintf(stderr,"unknown error while reading SWF\n");
				return 0;
			}
		}

		return length+8;
	}

	int File::save(FILE *fp, Context *_ctx) {
		Context *ctx;
		ctx = _ctx ? _ctx : new Context;

		Writer* w = NULL;
		unsigned char *data = NULL;

		if (!header.get()) {
			fprintf(stderr,"no SWF loaded to save\n");
			goto fail;
		}

		if (compressed) {
			fwrite("CWS", 3, 1, fp);
		} else {
			fwrite("FWS", 3, 1, fp);
		}

		fwrite(&version, 1, 1, fp);

		length+=8;
		fputc(length&0xFF, fp);
		fputc((length>>8)&0xFF, fp);
		fputc((length>>16)&0xFF, fp);
		fputc((length>>24)&0xFF, fp);
		length-=8;

		data = new unsigned char[length];
		if (!data) {
			fprintf(stderr,"ERROR: cannot save SWF to memory (size %i)\n",length);
			goto fail;
		}

		ctx->swfVersion = version;

		w = new Writer(data, length);
		header->write(w, ctx);
		if(w->getError() != SWFW_OK) {
			goto fail;
		}

		if (compressed) {
			compress(data, length, fp);
		} else {
			if (fwrite( data, length, 1, fp) != 1 ) {
				fprintf(stderr,"ERROR: could not compress SWF to file (%i)\n",length);
				goto fail;
			}
		}

		delete w;
		delete[] data;

		if (!_ctx) {
			delete ctx;
		}

		return( length+8 );

	fail:
		delete w;
		delete[] data;

		if (!_ctx) {
			delete ctx;
		}

		return 0;
	}

	xmlDocPtr File::getXML(Context *ctx) {
		if (!ctx) {
			auto_ptr<Context> autoContext(new Context);
			return getXML(autoContext.get());
		}

		XmlDocAutoPtr doc;
		xmlNodePtr root;

		if (!header.get()) {
			fprintf(stderr,"no SWF loaded to save\n");
			return NULL;
		}

		doc = xmlNewDoc((const xmlChar*)"1.0");
		root = doc->xmlRootNode = xmlNewDocNode( doc, NULL, (const xmlChar *)"swf", NULL );

		// add version, compression
		char tmp[32];
		snprintf(tmp, 32, "%i", version);
		xmlSetProp(root, (const xmlChar *)"version", (const xmlChar *)tmp);
		snprintf(tmp, 32, "%i", compressed ? 1 : 0);
		xmlSetProp(root, (const xmlChar *)"compressed", (const xmlChar *)tmp);

		ctx->swfVersion = version;
		header->writeXML(root, ctx);

		return doc.release();
	}

	int File::saveXML(FILE *fp, Context *ctx) {
		XmlDocAutoPtr doc(getXML(ctx));

		if (!doc.get()) {
			return 0;
		}

		char *data = NULL;
		int size;

		xmlDocDumpFormatMemoryEnc(doc.get(), (xmlChar**)&data, &size, "UTF-8", 1);

		if (size) {
			auto_ptr<char> autoData(data);
			fwrite(autoData.get(), size, 1, fp);
		}

		return size;
	}

	int File::setXML(xmlNodePtr root, Context *ctx) {
		if (!ctx) {
			auto_ptr<Context> autoContext;
			return setXML(root, autoContext.get());
		}

		xmlNodePtr headerNode;

		if (strcmp((const char *)root->name, "swf")) {
			fprintf(stderr, "doesn't seem to be a swfml file\n");
			return 0;
		}

		if (header.get()) {
			fprintf(stderr, "SWF file already in memory, not loading XML\n");
			return 0;
		}

		headerNode = root->children;
		while (headerNode && (!headerNode->name || strcmp((const char *)headerNode->name, "Header"))) {
			headerNode = headerNode->next;
		}

		if (!headerNode) {
			fprintf(stderr, "swfml file is empty\n");
			return 0;
		}

		// get version, compression
		int t;
		XmlCharAutoPtr tmp(xmlGetProp(root, (const xmlChar *)"version"));
		if (tmp.get()) {
			sscanf((char *)tmp.get(), "%i", &t);
			ctx->swfVersion = version = t;
		}
		tmp = xmlGetProp(root, (const xmlChar *)"compressed");
		if (tmp.get()) {
			sscanf((char *)tmp.get(), "%i", &t);
			compressed = t>0;
		}

		header = auto_ptr<Header>(new Header);
		header->parseXML(headerNode, ctx);

		length = (header->getSize(ctx,0)/8);

		return length+8;
	}

	int File::loadXML(const char *filename, Context *ctx) {
		xmlNodePtr root;
		int length;

		XmlDocAutoPtr doc(xmlParseFile(filename));
		if (!doc.get()) {
			fprintf(stderr, "could not parse XML\n");
			return 0;
		}

		root = doc->xmlRootNode;
		length = setXML(root, ctx);

		return length;
	}

	#define MAX_BUFFER 1000000

	void File::compress(unsigned char *inputBuffer, size_t len, FILE *fp) {
		z_stream stream;
		static unsigned char outputBuffer[MAX_BUFFER];
		int status, count;

		stream.avail_in = len;
		stream.next_in = inputBuffer;
		stream.next_out = outputBuffer;
		stream.zalloc = (alloc_func) NULL;
		stream.zfree = (free_func) NULL;
		stream.opaque = (voidpf) 0;
		stream.avail_out = MAX_BUFFER;

		status = deflateInit(&stream, Z_BEST_COMPRESSION);
		if (status != Z_OK) {
			fprintf(stderr, "Error compressing SWF: %s\n", stream.msg);
			return;
		}

		while (true) {
			if (stream.avail_in == 0) {
				break;
			}

			status = deflate(&stream, Z_NO_FLUSH);

			if (status != Z_OK) {
				fprintf(stderr, "Error compressing SWF: %s\n", stream.msg);
				return;
			}

			count = MAX_BUFFER - stream.avail_out;
			if (count) {
				fwrite(outputBuffer, 1, count, fp);
			}

			stream.next_out = outputBuffer;
			stream.avail_out = MAX_BUFFER;
		}

		stream.next_out = outputBuffer;
		stream.avail_out = MAX_BUFFER;

		do {
			status = deflate(&stream, Z_FINISH);

			count = MAX_BUFFER - stream.avail_out;
			if (count) {
				fwrite(outputBuffer, 1, count, fp);
			}

			stream.next_out = outputBuffer;
			stream.avail_out = MAX_BUFFER;
		} while (status == Z_OK);

		if (status != Z_STREAM_END) {
			fprintf(stderr, "Error compressing SWF: %s\n", stream.msg);
			return;
		}

		status = deflateEnd(&stream);

		if (status != Z_OK) {
			fprintf(stderr, "Error compressing SWF: %s\n", stream.msg);
			return;
		}
	}

	void File::decompress(unsigned char *outputBuffer, size_t len, FILE *fp) {
		z_stream stream;
		static unsigned char inputBuffer[MAX_BUFFER];
		int status, count;

		stream.avail_in = 0;
		stream.next_in = inputBuffer;
		stream.next_out = outputBuffer;
		stream.zalloc = (alloc_func) NULL;
		stream.zfree = (free_func) NULL;
		stream.opaque = (voidpf) 0;
		stream.avail_out = len;

		status = inflateInit(&stream);
		if (status != Z_OK) {
			fprintf(stderr, "Error decompressing SWF: %s\n", stream.msg);
			return;
		}

		do {
			if (stream.avail_in == 0) {
				stream.next_in = inputBuffer;
				stream.avail_in = fread(inputBuffer, 1, MAX_BUFFER, fp);
			}

			if (stream.avail_in == 0) {
				break;
			}

			status = inflate(&stream, Z_SYNC_FLUSH);
		} while (status == Z_OK);

		if (status != Z_STREAM_END && status != Z_OK) {
			fprintf(stderr, "Error decompressing SWF: %s\n", stream.msg);
			return;
		}
	}

}
