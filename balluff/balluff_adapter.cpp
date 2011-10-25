//
//  main.c
//  cstream
//
//  Created by William Sobel on 10/17/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include <fstream>
#include <sstream>
#include <internal.hpp>
#include <stdio.h>
#include <string.h>
#include "stream.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <b64/cencode.h>
#include <b64/cdecode.h>
#include <zlib.h>
#include "balluff_adapter.hpp"
#include "balluff_serial.hpp"

#include "MurmurHash3.hpp"

using namespace std;

#define CHUNK 16384

inline char *intToHex(char *aBuffer, unsigned int aValue)
{
  static char table[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', 
                         '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  for (int i = 7; i >= 0; i--) {
    aBuffer[i] = table[aValue & 0xF];
    aValue >>= 4;
  }
  aBuffer[8] = '\0';
  return aBuffer;
}

uint32_t BalluffAdapter::computeHash(const std::string &aKey)
{
  uint32_t hash;
  MurmurHash3_x86_32(aKey.c_str(), aKey.length(), 0, &hash);
  return hash;
}


int BalluffAdapter::HandleXmlChunk(const char *xml, void *aObj)
{
  BalluffAdapter *adapter = (BalluffAdapter*) aObj;
  
  xmlDocPtr document;
  const char *path;
  xmlXPathContextPtr xpathCtx;
  xmlNodePtr root;
  xmlXPathObjectPtr nodes;
  xmlNodeSetPtr nodeset;
  int i;
  
  document = xmlReadDoc(BAD_CAST xml, "file://node.xml",
                        NULL, XML_PARSE_NOBLANKS);
  if (document == NULL) 
  {
    gLogger->error("Cannot parse document: %s\n", xml);
    xmlFreeDoc(document);
    return 0;
  }
  
  path = "//m:Events/*";
  xpathCtx = xmlXPathNewContext(document);
  
  root = xmlDocGetRootElement(document);
  if (root->ns != NULL)
  {
    xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href);
  }
  else
  {
    gLogger->error("Document does not have a namespace: %s\n", xml);
    xmlFreeDoc(document);
    return 0;
  }
  
  // Evaluate the xpath.
  nodes = xmlXPathEval(BAD_CAST path, xpathCtx);
  if (nodes == NULL || nodes->nodesetval == NULL)
  {
    printf("No nodes found matching XPath\n");
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(document);
    return 1;
  }
  
  // Spin through all the events, samples and conditions.
  nodeset = nodes->nodesetval;
  for (i = 0; i != nodeset->nodeNr; ++i)
  {
    xmlNodePtr n = nodeset->nodeTab[i];
    xmlChar *name = xmlGetProp(n, BAD_CAST "name");
    xmlChar *value;
    
    if (name == NULL)
      name = xmlGetProp(n, BAD_CAST "dataItemId");
    value = xmlNodeGetContent(n);
    
    if (xmlStrcmp(n->name, BAD_CAST "AssetChanged") == 0 && 
        xmlStrcmp(value, BAD_CAST "UNAVAILABLE") != 0) {
      adapter->sendAssetToAgent((const char*) value);
    }    
    xmlFree(value);
    xmlFree(name);
  }
  
  xmlXPathFreeObject(nodes);    
  xmlXPathFreeContext(xpathCtx);
  xmlFreeDoc(document);
  
  return 1;
}

string BalluffAdapter::getAsset(std::string &aUrl, const char *aId)
{
  // Get the XML for the asset from the server.
  string str = aUrl + "assets/" + aId;
  void *context = MTCWebRequest(str.c_str());
  string xml;
  const char *result;
  if (MTCWebExecute(context, &result) == 0) {
    xml = result;
  }
  MTCWebFree(context);

  return xml;
}


void BalluffAdapter::sendAssetToAgent(const char *aId)
{
  // Get the XML for the asset from the server.
  string result = getAsset(mBaseUrl, aId);
  if (!result.empty()) {
    map<string,string> attrs = getAttributes(result, "//m:CuttingTool");
    
    if (attrs.count("deviceUuid") > 0 && mDevice != attrs["deviceUuid"]) {
      mOutgoingSize = encodeResult(result.c_str(), mOutgoing);
      
      // Make sure we are writing back to the correct RFID, if it has changed, then
      // skip this write. The one alternative is if we are overwriting this id
      // in which case it will have a status of only new and the deviceUuid will be
      // ???? (TODO: need to figure out what the uuid should be)
      
      mOutgoingId = aId;
      mOutgoingTimestamp = attrs["timestamp"];
      
      if (attrs["deviceUuid"] == "NEW")
        mForceOverwrite = true;
      
      //gLogger->debug("Sending asset:\n %s", result.c_str());
      addAsset(aId, attrs["element"].c_str(), result.c_str()); 
    }
  }
}

map<string,string> BalluffAdapter::getAttributes(const string &aXml, 
                                                const string &aXPath)
{
  std::map<std::string,std::string> res;
  
  xmlDocPtr document = xmlReadDoc(BAD_CAST aXml.c_str(), "file://node.xml",
                                    NULL, XML_PARSE_NOBLANKS);
  if (document == NULL) {
    gLogger->error("Cannot parse document: %s\n", aXml.c_str());
  } else {
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(document);
    
    xmlNodePtr root = xmlDocGetRootElement(document);
    if (root->ns != NULL)
    {
      xmlXPathRegisterNs(xpathCtx, BAD_CAST "m", root->ns->href);
      
      xmlXPathObjectPtr nodes;
      xmlNodeSetPtr nodeset;
      
      // Evaluate the xpath.
      nodes = xmlXPathEval(BAD_CAST aXPath.c_str(), xpathCtx);
      if (nodes == NULL || nodes->nodesetval == NULL || nodes->nodesetval->nodeTab == NULL)
      {
        printf("No nodes found matching XPath\n");
      } else {
        // Spin through all the events, samples and conditions.
        nodeset = nodes->nodesetval;
        xmlNodePtr node = nodeset->nodeTab[0];
        
        res["element"] = (const char*) (node->name);
        for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next)
        {
          if (attr->type == XML_ATTRIBUTE_NODE) {
            res[(const char *) attr->name] = (const char*) attr->children->content;
          }
        }
        
        xmlXPathFreeObject(nodes);
      }
      
      xmlXPathFreeContext(xpathCtx);
    }
    else
    {
      gLogger->error("Document does not have a namespace: %s\n", aXml.c_str());
    }
  }

  if (document != NULL)    
    xmlFreeDoc(document);

  return res;
}

BalluffAdapter::EChanged BalluffAdapter::hasAssetChanged(map<string,string> &aAttributes)
{
  if (aAttributes.count("assetId") < 1 || aAttributes.count("timestamp") < 1)
    return ERROR;
  
  if (aAttributes["assetId"] == mCurrentAssetId && 
      aAttributes["timestamp"] == mCurrentAssetTimestamp)
    return SAME;
  
  string &id = aAttributes["assetId"];
  string xml = getAsset(mMyBaseUrl, id.c_str());
  if (!xml.empty()) {
    map<string,string> attrs = getAttributes(xml, "//m:CuttingTool");
    if (attrs.count("assetId") > 0 && attrs.count("timestamp") > 0 && 
        attrs["timestamp"] == aAttributes["timestamp"] && 
        attrs["assetId"] == aAttributes["assetId"])
      return SAME;
    else
      return CHANGED;
  }
  
  return ERROR;
}


static void StreamXMLErrorFunc(void *ctx ATTRIBUTE_UNUSED, const char *msg, ...)
{
  va_list args;
  char buffer[2048];
  va_start(args, msg);
  vsnprintf(buffer, 2046, msg, args);
  buffer[2047] = '0';
  va_end(args);
  
  gLogger->error("XML Error: %s\n", buffer);   
}

uint16_t BalluffAdapter::encodeResult(const char *aData, uint8_t *aEncoded)
{
  int ret;
  uLongf len;

  len = MAX_RFID_SIZE;
  *((uint16_t*) aEncoded) = htons(strlen(aData));
  len -= sizeof(uint16_t);

  ret = compress(aEncoded + sizeof(uint16_t), &len, 
                 (Bytef*) aData, strlen(aData));
  if (ret != Z_OK)
    return 0;
  
  // Add the lenght of the short length.
  len += sizeof(uint16_t);
      
  return (uint16_t) len;
}

char *BalluffAdapter::reconstitute(const uint8_t *aText, uint16_t aSize)
{
  unsigned char *xml;
  uLongf size;
  
  size = ntohs(*((uint16_t*) aText)) + 32;
  xml = (unsigned char*) malloc(size);

  int ret = uncompress(xml, &size, (Bytef*) aText + sizeof(uint16_t), aSize - sizeof(uint16_t));
  
  if (ret != Z_OK)
  {
    free(xml);
    return NULL;
  }

  xml[size] = '\0';
  return (char*) xml;
}

BalluffAdapter::BalluffAdapter(int aPort)
  : Adapter(aPort, 1000), mAvailability("avail")
{
  addDatum(mAvailability);
}

BalluffAdapter::~BalluffAdapter()
{
  delete mSerial;
}

void BalluffAdapter::initialize(int aArgc, const char *aArgv[])
{
  MTConnectService::initialize(aArgc, aArgv);
  
  string file = "adapter.yaml";
  if (aArgc > 0)
  {
    file = aArgv[0];
  }
  
  ifstream str(file.c_str());
  mConfiguration.parse(str);
  
  mDevice = mConfiguration.getDeviceUuid();
  mUrl = mConfiguration.getAgentUrl();
  mBaseUrl = mConfiguration.getBaseAgentUrl();
  mMyBaseUrl = mConfiguration.getMyBaseAgentUrl();
    
  mPort = mConfiguration.getPort();
  mScanDelay = mConfiguration.getScanDelay();
  mHeartbeatFrequency = mConfiguration.getTimeout();

  mSerial = new BalluffSerial(mConfiguration.getSerialPort().c_str(),
                              mConfiguration.getBaud(),
                              mConfiguration.getParity().c_str(), 
                              mConfiguration.getDataBits(),
                              mConfiguration.getStopBits());

  gLogger->setLogLevel(Logger::eDEBUG);
}

void *BalluffAdapter::AgentMonitor(void *anAdapter)
{
  ((BalluffAdapter*) anAdapter)->agentMonitor();
  static int res = 0;
  return &res;
}

void BalluffAdapter::startAgentMonitor()
{
  int res = pthread_create(&mServerThread, NULL, AgentMonitor, this);
  if (res != 0) {
    gLogger->error("Cannot create server thread");
    exit(1);
  }
}

void BalluffAdapter::agentMonitor()
{
  xmlInitParser();
  xmlXPathInit();
  xmlSetGenericErrorFunc(NULL, StreamXMLErrorFunc);
  
  // Start from the current position...
  while (true)
  {
    void *currentContext = MTCWebRequest((mBaseUrl + "current").c_str());
    const char *current;
    if (MTCWebExecute(currentContext, &current) == 0)
    {
      // Get the last position we should start from...
      map<string,string> attrs = getAttributes(current, "//m:Header");
      if (attrs.count("nextSequence") > 0) 
      {
        string url = mUrl;
        if (url[url.length() - 1] != '/') url.append("/");
        url.append("sample?interval=10&path=//DataItem[@type=\"ASSET_CHANGED\"]&"
                   "from=" + attrs["nextSequence"]);
        void *context = MTCStreamInit(url.c_str(), HandleXmlChunk, this);
        MTCStreamStart(context);
        MTCStreamFree(context);
      }
    }
    MTCWebFree(currentContext);      
    sleepMs(1000);
  }
}

bool BalluffAdapter::checkForNewAsset(uint32_t aHash)
{
  bool ret;
  if (aHash != mCurrentHash) {
    mCurrentAssetId.clear();
    mCurrentAssetTimestamp.clear();
    mCurrentHash = 0;
    mHasHash = false;
    ret = true;
  } else {
    ret = false;
  }
  
  return ret;
}

bool BalluffAdapter::readAssetFromRFID(uint32_t aHash)
{
  uint8_t type;
  uint16_t size;
  if (mSerial->readHeader(size, type) != BalluffSerial::SUCCESS)
    return false;
  
  if (size > MAX_RFID_SIZE)
    return false;
  
  uint8_t incoming[MAX_RFID_SIZE];
  memset(incoming, 0, sizeof(incoming));
  BalluffSerial::EResult res = mSerial->readRFID(incoming, size);
  mHasHash = false;

  if (res == BalluffSerial::SUCCESS) {
    if (type == 'G') {
      char *decode = reconstitute(incoming, size);
      if (decode != NULL) {
        // Check for XML
        map<string,string> attrs = getAttributes(decode, "//m:CuttingTool");
        EChanged chg = hasAssetChanged(attrs);
        if (chg != ERROR) {
          if (chg == CHANGED) {
            gLogger->debug("Sending %s to agent", attrs["assetId"].c_str());
            addAsset(attrs["assetId"].c_str(), attrs["element"].c_str(), decode);
          } else {
            gLogger->debug("Asset has remained the same");
          }                
          mCurrentAssetId = attrs["assetId"];
          mCurrentAssetTimestamp = attrs["timestamp"];
          string key = mCurrentAssetId + mCurrentAssetTimestamp;
          mCurrentHash = computeHash(key);
          if (aHash != mCurrentHash)
            gLogger->error("Hash codes don't match: %x != %x", mCurrentHash, aHash);
          mHasHash = true;
        } else {              
          gLogger->warning("Asset data did not contain necessary data");
        }
        free(decode);
      } else {
        gLogger->error("Error decoding gzip data");
      }
    } else if (type == 'U') {
      // We have a URI and the uri is different from our own...
      //sendAssetToAgent(incoming.c_str());
    }    
  } else {
    mSerial->reset();
  }
    
  
  return true;
}

bool BalluffAdapter::writeAssetToRFID(uint32_t aHash)
{  
  bool ret = false;
  BalluffSerial::EResult res= mSerial->writeRFID(aHash,'G', mOutgoing, 
                                                 mOutgoingSize);
  if (res == BalluffSerial::SUCCESS)
  {
    cout << "Succussfully wrote: " << mOutgoing << endl;
    ret = true;
  } else {
    // We may not have had enough room...
    
    string url = mBaseUrl + "assets/" + mOutgoingId;
    strcpy((char*) mOutgoing, url.c_str());
    mOutgoingSize = strlen((char*) mOutgoing);
    res= mSerial->writeRFID(aHash, 'U', mOutgoing, mOutgoingSize);
    if (res == BalluffSerial::SUCCESS) {
      cout << "Succussfully wrote: " << mOutgoing << endl;
      ret = true;
    }    
  } 
  
  mOutgoingSize = 0;
  
  return ret;
}

bool BalluffAdapter::checkNewOutgoingAsset(uint32_t &aHash)
{
  // Calculate our hash key
  string key = mOutgoingId + mOutgoingTimestamp;
  aHash = computeHash(key);
  
  // If we are forcing a new asset identity onto this tool,
  // overwrite.
  bool ret = true;
  if (!mForceOverwrite) {
    // Have we read the header yet?
    
    // Need to do a read first
    if (!mHasHash) {
      // Do nothing, return will be false. We'll come back here after the data is
      // read and the hash code is populated.
      ret = false;
    } else if (aHash == mCurrentHash) {
      // Is this the same data that's already on the asset? 
      gLogger->info("Attempt to write identical data to the data carrier: %s", mOutgoingId.c_str());
      mOutgoingSize = 0;
      ret = false;
    } else if (mCurrentAssetId != mOutgoingId) {
      // Also make sure we are writing the same asset. the asset id must be the same.
      gLogger->info("Asset id %s skipped because it is not the current asset on the data carrier: %s",
                    mOutgoingId.c_str(), mCurrentAssetId.c_str());
      mOutgoingSize = 0;
      ret = false;
    }
  }
  
  return ret;
}

bool BalluffAdapter::checkForDataCarrier(uint32_t &aHash)
{
  int head;
  bool ret = false;
  BalluffSerial::EResult res = mSerial->checkForData(head, aHash);
  if (res == BalluffSerial::SUCCESS ) {      
    if (head == 0)
      mHasHash = false;
    else 
      ret = true;
  } else {
    mHasHash = false;
    mSerial->reset();
  }
  
  return ret;
}

void BalluffAdapter::start()
{
  mForceOverwrite = false;
  mOutgoingSize = 0;
  
  // Start agent heartbeat thread...
  startServerThread();
  
  startAgentMonitor();
  
  // Connect serial connection.
  mSerial->connect();
  mSerial->flush();
  
  begin();
  mBuffer.reset();
  mBuffer.timestamp();
  
  mAvailability.available();
  
  sendChangedData();
  cleanup();

  // Lets try some simple communication...
  while (mSerial->reset() != BalluffSerial::SUCCESS ||
         mSerial->selectHead(1) != BalluffSerial::SUCCESS) {
    mSerial->flush();
    sleepMs(1000);
  }

  while (true)
  {
    mSerial->flush();
    
    uint32_t hash;
    if (checkForDataCarrier(hash)) {      
      if (checkForNewAsset(hash)) {
        readAssetFromRFID(hash);
      }

      if (mOutgoingSize != 0 && (mForceOverwrite || mHasHash)) {
        uint32_t hash;
        if (checkNewOutgoingAsset(hash))
          if (!writeAssetToRFID(hash))
            mSerial->reset();
        mForceOverwrite = false;
      }
    }
    
    sleepMs(1000);
  }    
}

void BalluffAdapter::stop()
{
  stopServer();
}

void BalluffAdapter::gatherDeviceData()
{
}

