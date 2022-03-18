tag=dev
image=paskalmaksim/nginx-push-stream-module:$(tag)

build:
	docker build --pull . -t $(image)
push:
	docker push $(image)
run:
	docker run -it --rm -p 8080:80 $(image)
scan:
	trivy image \
	-ignore-unfixed --no-progress --severity HIGH,CRITICAL \
	$(image)