build:
	docker build . -t paskalmaksim/nginx-push-stream-module:dev
push:
	docker push paskalmaksim/nginx-push-stream-module:dev
run:
	docker run -it --rm -p 8080:80 paskalmaksim/nginx-push-stream-module:dev