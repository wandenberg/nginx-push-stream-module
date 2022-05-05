tag=dev
image=paskalmaksim/nginx-push-stream-module:$(tag)

build:
	docker build --pull . -t $(image)
push:
	docker push $(image)
run:
	docker run -it -u 30000 --rm -p 8080:8080 $(image)
scan:
	trivy image \
	-ignore-unfixed --no-progress --severity HIGH,CRITICAL \
	$(image)