/*------------------------------------------------------------------------------
* Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team
* 
* Distributable under the terms of either the Apache License (Version 2.0) or 
* the GNU Lesser General Public License, as specified in the COPYING file.
------------------------------------------------------------------------------*/
#include "CLucene/_ApiHeader.h"
#include "_FieldsWriter.h"

//#include "CLucene/util/VoidMap.h"
#include "CLucene/util/CLStreams.h"
#include "CLucene/util/Misc.h"
#include "CLucene/store/Directory.h"
#include "CLucene/store/_RAMDirectory.h"
#include "CLucene/store/IndexOutput.h"
#include "CLucene/document/Document.h"
#include "CLucene/document/Field.h"
#include "_FieldInfos.h"

CL_NS_USE(store)
CL_NS_USE(util)
CL_NS_USE(document)
CL_NS_DEF(index)
	
FieldsWriter::FieldsWriter(Directory* d, const char* segment, FieldInfos* fn):
	fieldInfos(fn)
{
//Func - Constructor
//Pre  - d contains a valid reference to a directory
//       segment != NULL and contains the name of the segment
//Post - fn contains a valid reference toa a FieldInfos

	CND_PRECONDITION(segment != NULL,"segment is NULL");

	const char* buf = Misc::segmentname(segment,".fdt");
    fieldsStream = d->createOutput ( buf );
    _CLDELETE_CaARRAY( buf );

	CND_CONDITION(indexStream != NULL,"fieldsStream is NULL");
    
	buf = Misc::segmentname(segment,".fdx");
    indexStream = d->createOutput( buf );
    _CLDELETE_CaARRAY( buf );
      
	CND_CONDITION(indexStream != NULL,"indexStream is NULL");

	doClose = true;
}

FieldsWriter::FieldsWriter(CL_NS(store)::IndexOutput* fdx, CL_NS(store)::IndexOutput* fdt, FieldInfos* fn):
	fieldInfos(fn)
{
	CND_CONDITION(fieldsStream != NULL,"fieldsStream is NULL");
	fieldsStream = fdt;
	CND_CONDITION(indexStream != NULL,"indexStream is NULL");
	indexStream = fdx;
	doClose = false;
}

FieldsWriter::~FieldsWriter(){
//Func - Destructor
//Pre  - true
//Post - Instance has been destroyed

	close();
}

void FieldsWriter::close() {
//Func - Closes all streams and frees all resources
//Pre  - true
//Post - All streams have been closed all resources have been freed

	if (! doClose )
		return;

	//Check if fieldsStream is valid
	if (fieldsStream){
		//Close fieldsStream
		fieldsStream->close();
		_CLDELETE( fieldsStream );
		}

	//Check if indexStream is valid
	if (indexStream){
		//Close indexStream
		indexStream->close();
		_CLDELETE( indexStream );
		}
}

void FieldsWriter::addDocument(Document* doc) {
//Func - Adds a document
//Pre  - doc contains a valid reference to a Document
//       indexStream != NULL
//       fieldsStream != NULL
//Post - The document doc has been added

	CND_PRECONDITION(indexStream != NULL,"indexStream is NULL");
	CND_PRECONDITION(fieldsStream != NULL,"fieldsStream is NULL");

	indexStream->writeLong(fieldsStream->getFilePointer());

	int32_t storedCount = 0;
	DocumentFieldEnumeration* fields = doc->getFields();
	while (fields->hasMoreElements()) {
		Field* field = fields->nextElement();
		if (field->isStored())
			storedCount++;
	}
	_CLLDELETE(fields);
	fieldsStream->writeVInt(storedCount);

	fields = doc->getFields();
	while (fields->hasMoreElements()) {
		Field* field = fields->nextElement();
		if (field->isStored()) {
			writeField(fieldInfos->fieldInfo(field->name()), field);
		}
	}
	_CLDELETE(fields);
}

void FieldsWriter::writeField(FieldInfo* fi, CL_NS(document)::Field* field)
{
	// if the field as an instanceof FieldsReader.FieldForMerge, we're in merge mode
	// and field.binaryValue() already returns the compressed value for a field
	// with isCompressed()==true, so we disable compression in that case
	//bool disableCompression = (field instanceof FieldsReader.FieldForMerge);

	fieldsStream->writeVInt(fieldInfos->fieldNumber(field->name()));
	uint8_t bits = 0;
	if (field->isTokenized())
		bits |= FieldsWriter::FIELD_IS_TOKENIZED;
	if (field->isBinary())
		bits |= FieldsWriter::FIELD_IS_BINARY;
	if (field->isCompressed())
		bits |= FieldsWriter::FIELD_IS_COMPRESSED;

	fieldsStream->writeByte(bits);

	if ( field->isCompressed() ){
		_CLTHROWA(CL_ERR_Runtime, "CLucene does not directly support compressed fields. Write a compressed byte array instead");
	}else{

		//FEATURE: this problem in Java Lucene too, if using Reader, data is not stored.
		//todo: this is a logic bug...
		//if the field is stored, and indexed, and is using a reader the field wont get indexed
		//
		//if we could write zero prefixed vints (therefore static length), then we could
		//write a reader directly to the field indexoutput and then go back and write the data
		//length. however this is not supported in lucene yet...
		//if this is ever implemented, then it would make sense to also be able to combine the
		//FieldsWriter and DocumentWriter::invertDocument process, and use a streamfilter to
		//write the field data while the documentwrite analyses the document! how cool would
		//that be! it would cut out all these buffers!!!


		// compression is disabled for the current field
		if (field->isBinary()) {
			//todo: since we currently don't support static length vints, we have to
			//read the entire stream into memory first.... ugly!
			InputStream* stream = field->streamValue();
			const signed char* sd;

			int32_t sz = stream->size();
			if ( sz < 0 )
				sz = 10000000; //todo: we should warn the developer here....

			//how do wemake sure we read the entire index in now???
			//todo: we need to have a max amount, and guarantee its all in or throw an error..
			//todo: make this value configurable....
			int32_t rl = stream->read(sd, sz, 1);

			if ( rl < 0 ){
				fieldsStream->writeVInt(0); //todo: could we detect this earlier and not actually write the field??
			}else{
				//todo: if this int could be written with a constant length, then
				//the stream could be read and written a bit at a time then the length
				//is re-written at the end.
				fieldsStream->writeVInt(rl);
				fieldsStream->writeBytes((uint8_t*)sd, rl);
			}

		}else if ( field->stringValue() == NULL ){ //we must be using readerValue
			CND_PRECONDITION(!field->isIndexed(), "Cannot store reader if it is indexed too")
			Reader* r = field->readerValue();

			int32_t sz = r->size();
			if ( sz < 0 )
				sz = 10000000; //todo: we should warn the developer here....

			//read the entire string
			const TCHAR* rv;
			int64_t rl = r->read(rv, sz, 1);
			if ( rl > LUCENE_INT32_MAX_SHOULDBE )
				_CLTHROWA(CL_ERR_Runtime,"Field length too long");
			else if ( rl < 0 )
				rl = 0;

			fieldsStream->writeString( rv, (int32_t)rl);
		}else if ( field->stringValue() != NULL ){
			fieldsStream->writeString(field->stringValue(),_tcslen(field->stringValue()));
		}else
			_CLTHROWA(CL_ERR_Runtime, "No values are set for the field");
	}
}

void FieldsWriter::flushDocument(int32_t numStoredFields, CL_NS(store)::RAMIndexOutput* buffer) {
	indexStream->writeLong(fieldsStream->getFilePointer());
	fieldsStream->writeVInt(numStoredFields);
	buffer->writeTo(fieldsStream);
}

void FieldsWriter::flush() {
  indexStream->flush();
  fieldsStream->flush();
}

/** Bulk write a contiguous series of documents.  The
*  lengths array is the length (in bytes) of each raw
*  document.  The stream IndexInput is the
*  fieldsStream from which we should bulk-copy all
*  bytes. */
void FieldsWriter::addRawDocuments(CL_NS(store)::IndexInput* stream, const int32_t* lengths, const int32_t numDocs) {
	int64_t position = fieldsStream->getFilePointer();
	const int64_t start = position;
	for(int32_t i=0;i<numDocs;i++) {
		indexStream->writeLong(position);
		position += lengths[i];
	}
	fieldsStream->copyBytes(stream, position-start);
	CND_CONDITION(fieldsStream->getFilePointer() == position,"fieldsStream->getFilePointer() != position");
}

CL_NS_END
