TOKEN_TTL = 86400
    CHANLLENGE_TTL = 300
    TOKEN: cache.MemoryStorage = cache.MemoryStorage()
    CHANLLENGES: cache.MemoryStorage = cache.MemoryStorage()
    VCMPS: cache.MemoryStorage = cache.MemoryStorage()
    DB = mongodb.get_database("vcmp_servers")

    async def query_vcmp_server(id: str) -> Optional[VCMPServer]:
        if VCMPS.exists(id):
            return VCMPS.get(id)
        data = await DB.get_collection("servers").find_one({"_id": ObjectId(id)})
        if data:
            server = VCMPServer(**data)
            VCMPS.set(id, server)
            return server
        return VCMPS.get(id)



    @app.get("/vcmp-agent/challenge")
    async def _(request: web.Request, id: str):
        server = await query_vcmp_server(id)
        if server is None:
            return {
                "status": False,
                "msg": "Server not found."
            }
        
        jwt_payload = hashlib.sha256(json.dumps({
            "timestamp": time.time(),
            "id": id
        }).encode("utf-8")).hexdigest()

        # 生成JWT
        jwt_header = base64.urlsafe_b64encode(b'{"alg":"HS256","typ":"JWT"}').decode('utf-8')
        jwt_payload = base64.urlsafe_b64encode(jwt_payload.encode('utf-8')).decode('utf-8')
        jwt_signature = base64.urlsafe_b64encode(hmac.new(server.secret.encode("utf-8"), (jwt_header + '.' + jwt_payload).encode('utf-8'), digestmod='sha256').digest()).decode('utf-8')
        jwt_token = jwt_header + '.' + jwt_payload + '.' + jwt_signature

        CHANLLENGES.set(id, jwt_token, CHANLLENGE_TTL)
        return {"status": True, "data": {"challenge": jwt_token}}
    
    def create_token(challenge, server: VCMPServer):
        jwt_payload = hashlib.sha256(json.dumps({
            "timestamp": time.time(),
            "id": server.secret
        }).encode("utf-8")).hexdigest()
        jwt_header = base64.urlsafe_b64encode(b'{"alg":"HS256","typ":"JWT"}').decode('utf-8')
        jwt_payload = base64.urlsafe_b64encode(jwt_payload.encode('utf-8')).decode('utf-8')
        jwt_signature = base64.urlsafe_b64encode(hmac.new(server.secret.encode("utf-8"), (jwt_header + '.' + jwt_payload + "." + challenge).encode('utf-8'), digestmod='sha256').digest()).decode('utf-8')
        jwt_token = jwt_header + '.' + jwt_payload + '.' + jwt_signature
        TOKEN.set(jwt_token, server.id, TOKEN_TTL)
        return jwt_token

    @app.post("/vcmp-agent/token")
    async def _(request: web.Request):
        data = await request.json()
        id = data['id']
        server = await query_vcmp_server(id)
        if server is None:
            return {
                "status": False,
                "msg": "Server not found."
            }
        challenge = data['challenge']
        signature = data['signature']
        if hmac.new(server.secret.encode("utf-8"), challenge.encode("utf-8"), hashlib.sha256).hexdigest() == signature:
            return {
                "status": True,
                "data": {
                    "token": create_token(challenge, server),
                    "ttl": TOKEN_TTL
                }
            }
        return {"status": False, "msg": "Error"}


    @app.websocket("/vcmp/socket")
    async def _(ws: web.WebSocket):
        vcmp = None
        async for raw_data in ws:
            logger.debug(raw_data)
            jdata = json.loads(raw_data)
            type = jdata["type"]
            data = jdata["data"]
            echo = jdata["echo"]
            response_template = {
                "type": type,
                "echo": echo,
            }
            if type == "auth":
                if TOKEN.exists(data):
                    vcmp = TOKEN.get(data)
                    await ws.send(json.dumps({
                        "status": True,
                        "data": vcmp,
                        **response_template
                    }))
                    continue
                await ws.send(json.dumps({
                    "status": False,
                    "msg": "Token not found.",
                    **response_template
                }))
                continue
            if vcmp is None:
                await ws.send(json.dumps({
                
                    "status": False,
                    "msg": "Forbidden.",
                    **response_template
                }))
            if type == "echo":
                await ws.send(json.dumps({
                    "status": True,
                    "data": data,
                    **response_template
                }))
        logger.debug("disconnect")