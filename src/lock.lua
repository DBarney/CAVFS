#!lua name=lock

local function unlock(keys, args)
	if redis.call("get",keys[1]) == args[1] then
		return redis.call("del",keys[1])
	else
		return 0
	end
end

redis.register_function('unlock',unlock)
