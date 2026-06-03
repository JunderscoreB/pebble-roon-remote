FROM node:18-alpine

WORKDIR /app

# Take 'package-bridge.json' from the NAS and rename it to 'package.json' inside the container
COPY package-roon_bridge.json ./package.json

# Install git so npm can clone the required GitHub dependencies
RUN apk add --no-cache git

# Install dependencies
RUN npm install

# Copy ONLY the bridge script into the container (ignores all your other NAS files)
COPY bridge-index.js ./

EXPOSE 3000

# Start the bridge
CMD ["node", "bridge-index.js"]
