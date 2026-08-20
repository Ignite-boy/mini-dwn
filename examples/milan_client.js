
class DwnClient {
  constructor(baseUrl){ this.baseUrl=baseUrl; }
  async processMessage(target, message, encodedData){
    const res = await fetch(`${this.baseUrl}/json-rpc`,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({jsonrpc:'2.0',id:Date.now().toString(),method:'dwn.processMessage',params:{target,message,encodedData}})});
    return res.json();
  }
  async recordsWrite(targetDid, data, opts={}){
    const dataStr = typeof data==='string'?data:JSON.stringify(data);
    const encodedData = Buffer.from(dataStr).toString('base64url');
    const descriptor={interface:'Records',method:'Write',dataFormat:opts.dataFormat||'application/json',dateCreated:new Date().toISOString(),dateModified:new Date().toISOString(),recordId:opts.recordId};
    const message={descriptor, authorization:{payload:'e30',signatures:[]}};
    return this.processMessage(targetDid,message,encodedData);
  }
}
module.exports={DwnClient};
